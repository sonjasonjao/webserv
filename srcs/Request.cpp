#include "../include/Request.hpp"

constexpr char const * const	CRLF = "\r\n";

/**
 * Probably more intuitive to initialize _isValid to false and _isMissingData to true.
 * Will need to make the whole logic support this.
 */
Request::Request(int fd) : _fd(fd), _keepAlive(false), _chunked(false), _isValid(true),
	_kickMe(false), _isMissingData(false) {
	_request.method = RequestMethod::Unknown;
}

/**
 * Saves the current buffer filled by recv into the combined buffer of this client.
 */
void	Request::saveRequest(std::string const& buf) {
	_buffer += buf;
}

/**
 * Checks whether the buffer so far includes "\r\n\r\n". If not, and headers is empty,
 * we assume the request is partial.
 *
 * Checking if headers is empty does not account for if there is something in headers, but
 * not yet the terminating "\r\n\r\n".
 */
void	Request::handleRequest(void) {
	if (_buffer.find("\r\n\r\n") == std::string::npos && _headers.empty())
		_isMissingData = true;
	else {
		_isMissingData = false;
		parseRequest();
	}
}

/**
 * After receiving and parsing a complete request, and handling it (= building a response and
 * sending it), we reset these properties of the current client for a possible following request.
 */
void	Request::reset(void) {
	_request.target.clear();
	_request.method = RequestMethod::Unknown;
	_request.httpVersion.clear();
	_request.query.reset();
	_headers.clear();
	_body.clear();
	_contentLen.reset();
	_isMissingData = false;
	_chunked = false;
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
	if (key == "host" || key == "content-length" || key == "content-type" || key == "connection"
		|| key == "transfer-encoding")
		return true;
	return false;
}

/**
 * Validates and parses different sections of the request. After the last valid
 * header line, if there is remaining data, and content-length is found in headers, that length
 * of data is stored in _body.
 */
void	Request::parseRequest(void) {
	if (_request.method == RequestMethod::Unknown) {
		std::string	reqLine = splitReqLine(_buffer, CRLF);
		std::istringstream	req(reqLine);
		parseRequestLine(req);
		if (!_isValid)
			return ;
	}
	if (_headers.empty())
		parseHeaders(_buffer);
	if (!_isValid || _kickMe)
		return ;
	if (!_buffer.empty() && (_contentLen.has_value() && _body.size() < _contentLen.value())) {
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
			_isMissingData = true;
		else if (_body.size() == _contentLen.value())
			_isMissingData = false;
	}
	else if (_chunked)
		parseChunked();
	printData();
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
		_isValid = false;
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
			_isValid = false;
			return ;
	}
	if (!isTargetValid(target) || !isHttpValid(httpVersion))
	{
		_isValid = false;
		return ;
	}
}

/**
 * Accepts as headers every line with ':' and stores each header as key and value to
 * an unordered map. For now, only stores the needed headers and skips the rest.
 *
 * IMPLEMENT CHECK IF HEADERS END WITH \r\n\r\n, OTHERWISE INVALID!
 */
void	Request::parseHeaders(std::string& str) {
	std::string	line;
	while (!str.empty()) {
		if (str.substr(0, 2) == CRLF) {
			str = str.substr(2);
			break ;
		}
		line = splitReqLine(str, CRLF);
		const size_t point = line.find(":");
		if (point == std::string::npos) {
			_kickMe = true;
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
	if (_headers.empty() || !validateHeaders()) {
		_isValid = false;
		return ;
	}
}

/**
 * In the case of a chunked request, attempts to check the size of each chunk and split the
 * string accordingly to store that chunk into body. Still need to consider what happens if the
 * given size of chunk differs from the actual size.
 */
void	Request::parseChunked(void) {
	while (!_buffer.empty()) {
		auto	pos = _buffer.find(CRLF);
		auto	finder = _buffer.find("0\r\n\r\n");
		if (finder == std::string::npos && pos != std::string::npos) {
			while (pos != std::string::npos) {
				size_t	len = std::stoi(_buffer.substr(0, pos), 0, 16);
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = splitReqLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_isValid = false;
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_isMissingData = true;
		}
		else if (finder != std::string::npos) {
			while (_buffer.substr(pos - 1, 5) != "0\r\n\r\n") {
				size_t	len = std::stoi(_buffer.substr(0, pos), 0, 16);
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = splitReqLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_isValid = false;
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_body += splitReqLine(_buffer, "0\r\n\r\n");
			_isMissingData = false;
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
	if (it == _headers.end() || it->second.empty())
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
	if (it != _headers.end())
		_contentLen = std::stoi(it->second.front());
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
 * In case the URI includes '?', we use it as a separator to get the query.*
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

/**
 * Prints parsed data just for debugging for now.
 */
void	Request::printData(void) const {
	std::cout << "----Request line:----\nMethod: ";
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
		<< _request.target << ", HTTP version: " << _request.httpVersion << '\n';
	if (_request.query.has_value())
		std::cout << "Query: " << _request.query.value() << '\n';
	std::cout << "----Header keys----:\n";
	for (auto it = _headers.begin(); it != _headers.end(); it++)
		std::cout << it->first << '\n';
	if (!_body.empty())
		std::cout << "----Body:----\n" << _body << '\n';
	std::cout << "----Keep alive?---- " << _keepAlive << '\n';
	std::cout << "----Missing data?---- " << _isMissingData << '\n';
	std::cout << "----Chunked?---- " << _chunked << '\n';
	std::cout << "----Valid?----" << _isValid << '\n';
}

std::string	Request::getHost(void) {
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

bool	Request::getKeepAlive(void) const {
	return _keepAlive;
}

bool	Request::getIsValid(void) const {
	return _isValid;
}

bool	Request::getIsMissingData(void) const {
	return _isMissingData;
}

bool	Request::getKickMe(void) const {
	return _kickMe;
}

bool	Request::isBufferEmpty(void) {
	if (_buffer.empty())
		return true;
	return false;
}
