#include "../include/Request.hpp"
#include "../include/Log.hpp"
#include <regex>
#include <sstream>
#include <iostream>
#include <unordered_set>

constexpr char const * const	CRLF = "\r\n";

Request::Request(int fd, int serverFd) : _fd(fd), _serverFd(serverFd), _keepAlive(false), _chunked(false), _completeHeaders(false) {
	_request.method = RequestMethod::Unknown;
	_status = ReqStatus::WaitingData;
}

/**
 * Saves the current buffer filled by recv into the combined buffer of this client.
 */
void	Request::saveRequest(std::string const& buf) {
	_buffer += buf;
}

/**
 * Checks whether the buffer so far includes "\r\n\r\n". If not, and the headers part hasn't
 * been received completely (ending with "\r\n\r\n"), we assume the request is partial.
 */
void	Request::handleRequest(void) {
	if (_buffer.find("\r\n\r\n") == std::string::npos && !_completeHeaders)
		_status = ReqStatus::WaitingData;
	else
		parseRequest();
}

/**
 * After receiving and parsing a complete request, and handling it (= building a response and
 * sending it), these properties of the current client are reset for a possible following request.
 */
void	Request::reset(void) {
	_request.target.clear();
	_request.method = RequestMethod::Unknown;
	_request.httpVersion.clear();
	_request.query.reset();
	_headers.clear();
	_body.clear();
	_contentLen.reset();
	_status = ReqStatus::WaitingData;
	_chunked = false;
	_completeHeaders = false;
}

/**
 * Resets the keepAlive status separately from other resets, only after keepAlive status of the
 * latest request has been checked.
*/
void	Request::resetKeepAlive(void) {
	_keepAlive = false;
}

/**
 * Helper to split the buffer with delimiter. Returns the current line until
 * the delimiter and updates _buffer by removing that extracted string.
 */
std::string	splitReqLine(std::string& orig, std::string delim)
{
	auto it = orig.find(delim);
	std::string tmp;
	if (it != std::string::npos)
	{
		tmp = orig.substr(0, it);
		if (orig.size() > it + delim.size())
			orig = orig.substr(it + delim.size());
		else
			orig.erase();
	}
	return tmp;
}

/**
 * For now, we only store in _headers these headers, as they are needed for the server. Will
 * have to figure out if we need others, and if we should still store and validate the others,
 * too.
 */
bool	isNeededHeader(std::string& key)
{
	if (key == "host" || key == "content-length" || key == "content-type"
		|| key == "connection" || key == "transfer-encoding")
		return true;
	return false;
}

/**
 * Validates and parses different sections of the request. After the last valid
 * header line, if there is remaining data, and content-length is found in headers, that length
 * of data is stored in _body - if chunked is found in headers, the rest is handled as chunks.
 */
void	Request::parseRequest(void) {
	if (_request.method == RequestMethod::Unknown) {
		std::string	reqLine = splitReqLine(_buffer, CRLF);
		std::istringstream	req(reqLine);
		parseRequestLine(req);
		if (_status == ReqStatus::Invalid) {
			_buffer.clear();
			return ;
		}
	}
	if (!_completeHeaders)
		parseHeaders(_buffer);
	if (_status == ReqStatus::Invalid || _status == ReqStatus::Error) {
		_buffer.clear();
		return ;
	}
	if (_buffer.empty() && !_contentLen.has_value() && !_chunked)
		_status = ReqStatus::CompleteReq;
	else if (!_buffer.empty() && (_contentLen.has_value() && _body.size() < _contentLen.value())) {
		size_t	missingLen = _contentLen.value() - _body.size();
		if (missingLen < _buffer.size()) {
			std::string	toAdd = _buffer.substr(0, missingLen);
			_body += toAdd;
			_buffer = _buffer.substr(missingLen);
		}
		else {
			_body += _buffer;
			_buffer.clear();
		}
		if (_body.size() < _contentLen.value())
			_status = ReqStatus::WaitingData;
		else if (_body.size() == _contentLen.value())
			_status = ReqStatus::CompleteReq;
	}
	else if (_chunked)
		parseChunked();
	printData();
}

/**
 * In case of invalid request flagged before header parsing, host is searched from the buffer in
 * order to store it for forming a 400 Bad request response.
 */
void	Request::fillHost(void) {
	auto	pos = _buffer.find("Host: ");
	if (pos == std::string::npos)
		pos = _buffer.find("host: ");
	if (pos != std::string::npos) {
		std::string	key = "host";
		size_t	valueStart = pos + 6;
		auto	valueEnd = _buffer.find("\r\n", valueStart);
		if (valueEnd == std::string::npos)
			valueEnd = _buffer.size();
		std::string	value = _buffer.substr(valueStart, valueEnd - valueStart);
		_headers[key].push_back(value);
	}
	//if Host header is not found, what will we do with the response?
}

/**
 * Splits the request line into tokens, recognises method, and validates target path
 * and HTTP version.
 */
void	Request::parseRequestLine(std::istringstream& req) {
	std::string method, target, httpVersion;
	std::vector<std::string>	methods = { "GET", "POST", "DELETE "};
	if (!(req >> method >> target >> httpVersion))
	{
		_status = ReqStatus::Invalid;
		fillHost();
		return ;
	}
	size_t i ;
	for (i = 0; i < methods.size(); i++)
	{
		if (methods[i] == method)
			break ;
	}
	switch (i)
	{
		case 0:
			_request.method = RequestMethod::Get;
			break ;
		case 1:
			_request.method = RequestMethod::Post;
			break ;
		case 2:
			_request.method = RequestMethod::Delete;
			break ;
		default:
			_status = ReqStatus::Invalid;
			fillHost();
			return ;
	}
	if (!isTargetValid(target) || !isHttpValid(httpVersion))
	{
		_status = ReqStatus::Invalid;
		fillHost();
		return ;
	}
}

/**
 * Accepts as headers every line with ':' and stores each header as key and value to
 * an unordered map. For now, only stores the needed headers and skips the rest.
 */
void	Request::parseHeaders(std::string& str) {
	std::string	line;
	while (!str.empty()) {
		if (str.substr(0, 2) == CRLF) {
			str = str.substr(2);
			_completeHeaders = true;
			break ;
		}
		line = splitReqLine(str, CRLF);
		const size_t point = line.find(":");
		if (point == std::string::npos) {
			_status = ReqStatus::Error;
			_keepAlive = false;
			return ;
		}
		std::string	key = line.substr(0, point);
		for (size_t i = 0; i < key.size(); i++)
			key[i] = std::tolower(static_cast<unsigned char>(key[i]));
		std::string value = line.substr(point + 2, line.size() - (point + 2));
		for (size_t i = 0; i < value.size(); i++)
			value[i] = std::tolower(static_cast<unsigned char>(value[i]));
		if (isNeededHeader(key)) {
			if (value.find(",") == std::string::npos) {
				_headers[key].push_back(value);
				continue;
			}
			std::istringstream	values(value);
			std::string	oneValue;
			while (getline(values, oneValue, ','))
				_headers[key].push_back(oneValue);
		}
	}
	if (!_completeHeaders)
		_status = ReqStatus::WaitingData;
	if (_headers.empty() || !validateHeaders()) {
		_status = ReqStatus::Invalid;
		return ;
	}
}

/**
 * In the case of a chunked request, attempts to check the size of each chunk and split the
 * string accordingly to store that chunk into body.
 */
void	Request::parseChunked(void) {
	while (!_buffer.empty()) {
		auto	pos = _buffer.find(CRLF);
		auto	finder = _buffer.find("0\r\n\r\n");
		if (finder == std::string::npos && pos != std::string::npos) {
			while (pos != std::string::npos) {
				size_t	len;
				try
				{
					len = std::stoi(_buffer.substr(0, pos), 0, 16);
				}
				catch (const std::exception& e)
				{
					_status = ReqStatus::Invalid;
					_buffer.clear();
					return;
				}
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = splitReqLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_status = ReqStatus::Invalid;
					_buffer.clear();
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_status = ReqStatus::WaitingData;
		}
		else if (finder != std::string::npos) {
			while (pos > 0 && _buffer.substr(pos - 1, 5) != "0\r\n\r\n") {
				size_t	len;
				try
				{
					len = std::stoi(_buffer.substr(0, pos), 0, 16);
				}
				catch (const std::exception& e)
				{
					_status = ReqStatus::Invalid;
					_buffer.clear();
					return;
				}
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = splitReqLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_status = ReqStatus::Invalid;
					_buffer.clear();
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_body += splitReqLine(_buffer, "0\r\n\r\n");
			_status = ReqStatus::CompleteReq;
		}
	}
}

/**
 * Checks if the headers include "host" (considered mandatory), and if unique headers only have one
 * value each (actually now we don't even store most of them, as they are not needed, so will
 * have to think whether we even need that either). Checks also for connection, to set keep-alive
 * flag if needed, and content-length, to set the length, and transfer-encoding for chunked flag.
 */
bool	Request::validateHeaders(void) {
	auto	it = _headers.find("host");
	if ((it == _headers.end() || it->second.empty()) && _request.httpVersion == "HTTP/1.1")
		return false;
	for (auto const& [key, values] : _headers) {
		if (values.size() > 1 && isUniqueHeader(key))
			return false;
	}
	it = _headers.find("connection");
	if (it != _headers.end()) {
		for (auto con = it->second.begin(); con != it->second.end(); con++) {
			if (*con == "keep-alive") {
				_keepAlive = true;
				break ;
			}
		}
	}
	it = _headers.find("content-length");
	if (it != _headers.end()) {
		try
		{
			_contentLen = std::stoi(it->second.front());
		}
		catch(const std::exception& e)
		{
			return false;
		}
	}
	it = _headers.find("transfer-encoding");
	if (it != _headers.end() && it->second.front() == "chunked")
		_chunked = true;
	return true;
}

/**
 * Defines headers that can have only one value, and checks if any of them has more.
 *
 * Need to double-check these.
 */
bool	Request::isUniqueHeader(std::string const& key) {
	std::unordered_set<std::string>	uniques = {
		"accept-datetime",
		"access-control-request-method",
		"authorization",
		"content-length",
		"content-md5",
		"date",
		"from",
		"host",
		"http2-settings",
		"if-modified-since",
		"if-range",
		"if-unmodified-since",
		"max-forwards",
		"origin",
		"pragma",
		"proxy-authorization",
		"referer",
		// "user-agent",
	};
	auto	it = uniques.find(key);
	if (it != uniques.end())
		return true ;
	return false ;
}

/**
 * Validates target path regarding characters.
 */
bool	Request::areValidChars(std::string& s) {
	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] < 32 || s[i] >= 127 || s[i] == '<' || s[i] == '>'
			|| s[i] == '"' || s[i] == '\\')
			return false ;
	}
	return true ;
}

/**
 * If target path is only one character, it has to be '/'.
 * If the target is given as absolute path, it has to be either of HTTP or HTTPS
 * protocol.
 *
 * In case the URI includes '?', we use it as a separator to get the query.
 * We must later split the possible query with '&' which separates different queries.
 * Might be better to do that in the CGI handling part, so for now it's all in one string.
 */
bool	Request::isTargetValid(std::string& target) {
	if (target.size() == 1 && target != "/")
		return false ;
	if (!areValidChars(target))
		return false ;
	size_t	protocolEnd = target.find("://");
	if (protocolEnd != std::string::npos)
	{
		std::string protocol = target.substr(0, protocolEnd);
		if (protocol != "http" && protocol != "https")
			return false ;
	}
	size_t	queryStart = target.find('?');
	if (queryStart != std::string::npos)
	{
		_request.target = target.substr(0, queryStart);
		_request.query = target.substr(queryStart + 1);
	}
	else
		_request.target = target;
	return true ;
}

/**
 * Only accepts HTTP/1.0 and HTTP/1.1 as valid versions on the request line.
 */
bool	Request::isHttpValid(std::string& httpVersion) {
	if (!std::regex_match(httpVersion, std::regex("HTTP/1.([01])")))
		return false ;
	_request.httpVersion = httpVersion;
	return true ;
}

void	printStatus(ReqStatus status)
{
	switch (status)
	{
		case ReqStatus::WaitingData:
			std::cout << "Waiting for more data\n";
			break ;
		case ReqStatus::CompleteReq:
			std::cout << "Complete and valid request received\n";
			break ;
		case ReqStatus::Error:
			std::cout << "Critical error found, client to be disconnected\n";
			break ;
		case ReqStatus::Invalid:
			std::cout << "Invalid HTTP request\n";
			break ;
		case ReqStatus::ReadyForResponse:
			std::cout << "Ready to receive response\n";
			break ;
		default:
			std::cout << "Unknown\n";
	}
}

/**
 * Prints parsed data just for debugging for now.
 */
void	Request::printData(void) const {
	std::cout << "---- Request line ----\nMethod: ";
	switch(_request.method) {
		case RequestMethod::Get:
			std::cout << "Get";
			break ;
		case RequestMethod::Post:
			std::cout << "Post";
			break ;
		case RequestMethod::Delete:
			std::cout << "Delete";
			break ;
		default:
			throw std::runtime_error("HTTP request method unknown\n");
	}
	std::cout << ", target: "
		<< _request.target << ", HTTP version: " << _request.httpVersion << "\n\n";
	if (_request.query.has_value())
		std::cout << "Query: " << _request.query.value() << '\n';
	std::cout << "---- Header keys ----\n";
	for (auto it = _headers.begin(); it != _headers.end(); it++)
		std::cout << it->first << '\n';
	std::cout << "\n";
	if (!_body.empty())
		std::cout << "---- Body ----\n" << _body << '\n';
	std::cout << "	Keep alive?		" << _keepAlive << '\n';
	std::cout << "	Complete headers?	" << _completeHeaders << '\n';
	std::cout << "	Status?		";
	printStatus(_status);
	std::cout << "	Chunked?		" << _chunked << '\n';
}

std::string	Request::getHost(void) const {
	std::string	host;
	try
	{
		host = (_headers.at("host")).front();
	}
	catch(const std::exception& e)
	{
		ERROR_LOG("unexpected error in matching request host to configuration");
	}
	return host;
}

int	Request::getFd(void) const {
	return _fd;
}

int	Request::getServerFd(void) const {
	return _serverFd;
}

bool	Request::getKeepAlive(void) const {
	return _keepAlive;
}

ReqStatus	Request::getStatus(void) const {
	return _status;
}

void	Request::setStatus(ReqStatus status) {
	_status = status;
}

std::string const	&Request::getBuffer(void) const {
	return _buffer;
}

RequestMethod	Request::getRequestMethod() const {
	return _request.method;
}

std::string const	&Request::getHttpVersion() const {
	return _request.httpVersion;
}

std::string const	&Request::getBody() const {
	return _body;
}

std::string const	&Request::getTarget() const {
	return _request.target;
}

std::vector<std::string> const *	Request::getHeader(std::string const &key) const
{
	try {
		return &_headers.at(key);
	} catch (std::exception const &e) {
		return nullptr;
	}
}
