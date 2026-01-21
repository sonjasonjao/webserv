#include "../include/Request.hpp"
#include "../include/Log.hpp"
#include <regex>
#include <iostream>
#include <unordered_set>
#include <chrono>

constexpr char const * const	CRLF = "\r\n";

/**
 * Initializes attribute values for request. Http version is now by default initialized to HTTP/1.1,
 * so if the http version given in the request is invalid, 1.1 will be used to send the error page
 * response.
 */
Request::Request(int fd, int serverFd) :
	_fd(fd),
	_serverFd(serverFd),
	_keepAlive(false),
	_chunked(false),
	_completeHeaders(false),
	_uploadFD(nullptr),
	_headerSize(0),
	_curr_upload_pos(0) {

	_request.method = RequestMethod::Unknown;
	_status = RequestStatus::WaitingData;
	_idleStart = std::chrono::high_resolution_clock::now();
	_recvStart = {};
	_sendStart = {};
	_request.httpVersion = "HTTP/1.1";
}

/**
 * Saves the current buffer filled by recv into the combined buffer of this client.
 */
void	Request::saveRequest(std::string const& buf) {
	_buffer += buf;
}

/**
 * Checks whether the buffer so far includes "\r\n\r\n". If not, and the headers section hasn't
 * been received completely (ending with "\r\n\r\n"), we assume the request is partial. In that
 * case, Host header is filled, so if the completing part of request never arrives, Host is
 * available for error page response forming.
 */
void	Request::handleRequest(void) {
	if (_buffer.find("\r\n\r\n") == std::string::npos && !_completeHeaders) {
		_status = RequestStatus::WaitingData;
	}
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
	_headerSize = 0;
	_body.clear();
	_contentLen.reset();
	_chunked = false;
	_completeHeaders = false;
	_recvStart = {};
	_curr_upload_pos = 0;
	if(_uploadFD) {
		_uploadFD->close();
		_uploadFD.reset();
	}
	_boundary.reset();
}

/**
 * Resets the keepAlive status separately from other resets, only after keepAlive status of the
 * latest request has been checked.
 */
void	Request::resetKeepAlive(void) {
	_keepAlive = false;
}

/**
 * In handleConnections, each client fd is checked for possible timeouts by comparing
 * the stored _idleStart, _recvStart, and _sendStart with the current time stamp. Helper
 * variable init is used to check whether _recvStart or _sendStart has ever been updated
 * after the initialization to zero.
 */
void	Request::checkReqTimeouts(void) {
	auto		now = std::chrono::high_resolution_clock::now();
	auto		diff = now - _idleStart;
	auto		durMs = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
	timePoint	init = {};
	if (durMs.count() > IDLE_TIMEOUT) {
		_status = RequestStatus::IdleTimeout;
		DEBUG_LOG("Idle timeout with client fd " + std::to_string(_fd));
		_keepAlive = false;
		return;
	}
	diff = now - _recvStart;
	durMs = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
	if (_recvStart != init && durMs.count() > RECV_TIMEOUT) {
		_status = RequestStatus::RecvTimeout;
		DEBUG_LOG("Recv timeout with client fd " + std::to_string(_fd));
		_keepAlive = false;
		return;
	}
	diff = now - _sendStart;
	durMs = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
	if (_sendStart != init && durMs.count() > SEND_TIMEOUT) {
		_status = RequestStatus::SendTimeout;
		DEBUG_LOG("Send timeout with client fd " + std::to_string(_fd));
		_keepAlive = false;
	}
}

/**
 * Helper to extract a string until delimiter from buffer. Returns the extracted string
 * until delimiter and updates _buffer by removing that extracted string.
 */
std::string	extractFromLine(std::string& orig, std::string delim)
{
	auto it = orig.find(delim);
	std::string tmp = "";
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
 * Validates and parses different sections of the request. After the last valid
 * header line, if there is remaining data, and content-length is found in headers, that length
 * of data is stored in _body - if chunked is found in headers, the rest is handled as chunks.
 */
void	Request::parseRequest(void) {
	if (_request.method == RequestMethod::Unknown) {

		// Ignoring empty lines (CRLF) before the request-line
		while (_buffer.substr(0, 2) == CRLF) {
			_buffer = _buffer.substr(2);
		}

		// If buffer is empty after skipping CRLFs, wait for more data
		if (_buffer.empty()) {
			return;
		}

		std::string	reqLine = extractFromLine(_buffer, CRLF);
		parseRequestLine(reqLine);
		if (_status == RequestStatus::Invalid) {
			_buffer.clear();
			return;
		}
	}
	if (!_completeHeaders)
		parseHeaders(_buffer);
	if (_status == RequestStatus::Invalid || _status == RequestStatus::Error) {
		_buffer.clear();
		return;
	}

	if (!_contentLen.has_value() && !_chunked)
		_status = RequestStatus::CompleteReq;
	else if(_request.method == RequestMethod::Post && _boundary.has_value()) {
		handleFileUpload();
	}
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
		if (_body.size() > CLIENT_MAX_BODY_SIZE) {
			_status = RequestStatus::ContentTooLarge;
			_keepAlive = false;
			_buffer.clear();
			return;
		}
		if (_body.size() < _contentLen.value())
			_status = RequestStatus::WaitingData;
		else if (_body.size() == _contentLen.value())
			_status = RequestStatus::CompleteReq;
	}
	else if (_chunked)
		parseChunked();
	printData();
}

/**
 * Splits the request line into tokens, recognises method, and validates target path
 * and HTTP version.
 */
void	Request::parseRequestLine(std::string &req) {
	std::string method, target, httpVersion;
	std::vector<std::string>	methods = { "GET", "POST", "DELETE" };
	method = extractFromLine(req, " ");
	target = extractFromLine(req, " ");
	httpVersion = req;
	if (method.empty() || target.empty() || httpVersion.empty()) {
		_status = RequestStatus::Invalid;
		return;
	}
	size_t i = 0;
	for (i = 0; i < methods.size(); i++)
	{
		if (methods[i] == method)
			break;
	}
	switch (i)
	{
		case 0:
			_request.method = RequestMethod::Get;
			break;
		case 1:
			_request.method = RequestMethod::Post;
			break;
		case 2:
			_request.method = RequestMethod::Delete;
			break;
		default:
			_status = RequestStatus::Invalid;
			return;
	}
	if (!validateAndAssignTarget(target) || !validateAndAssignHttp(httpVersion))
	{
		_status = RequestStatus::Invalid;
		return;
	}
}

/**
 * Accepts as headers every line with ':' and stores each header as key and value to
 * an unordered map.
 *
 * For Content-Type, string won't be turned to lowercase, because it might include
 * boundary= literal. Content-Type will be split by semicolons to store individual
 * values, and for other headers, split will be done with commas.
 */
void	Request::parseHeaders(std::string& str) {
	std::string	line;
	size_t	headEnd = str.find("\r\n\r\n");
	if (headEnd == std::string::npos)
		headEnd = str.size();
	_headerSize = headEnd;
	if (_headerSize > HEADERS_MAX_SIZE) {
		_status = RequestStatus::Invalid;
		_keepAlive = false;
		return;
	}
	while (!str.empty()) {
		if (str.substr(0, 2) == CRLF) {
			str = str.substr(2);
			_completeHeaders = true;
			break;
		}
		line = extractFromLine(str, CRLF);
		const size_t point = line.find(":");
		if (point == std::string::npos) {
			_status = RequestStatus::Error;
			_keepAlive = false;
			return;
		}
		std::string	key = line.substr(0, point);
		for (size_t i = 0; i < key.size(); i++)
			key[i] = std::tolower(static_cast<unsigned char>(key[i]));
		std::string value = line.substr(point + 2, line.size() - (point + 2));
		if (key != "content-type") {
			for (size_t i = 0; i < value.size(); i++)
				value[i] = std::tolower(static_cast<unsigned char>(value[i]));
		}
		if (_headers.find(key) != _headers.end() && isUniqueHeader(key)) {
			_status = RequestStatus::Invalid;
			_keepAlive = false;
			return;
		}
		std::istringstream	values(value);
		std::string	oneValue;
		if (key == "content-type") {
			if (value.find(";") == std::string::npos) {
				_headers[key].emplace_back(value);
				continue;
			}
			while (getline(values, oneValue, ';')) {
				if (!oneValue.empty() && oneValue[0] == ' ')
					oneValue = oneValue.substr(1);
				_headers[key].emplace_back(oneValue);
			}
		}
		else {
			if (value.find(",") == std::string::npos) {
				_headers[key].emplace_back(value);
				continue;
			}
			while (getline(values, oneValue, ',')) {
				if (!oneValue.empty() && oneValue[0] == ' ')
					oneValue = oneValue.substr(1);
				_headers[key].emplace_back(oneValue);
			}
		}
	}
	if (!_completeHeaders)
		_status = RequestStatus::WaitingData;
	if (_headers.empty() || !validateHeaders()) {
		_status = RequestStatus::Invalid;
		_keepAlive = false;
		return;
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
					_status = RequestStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = extractFromLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_status = RequestStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_status = RequestStatus::WaitingData;
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
					_status = RequestStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = extractFromLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_status = RequestStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_body += extractFromLine(_buffer, "0\r\n\r\n");
			_status = RequestStatus::CompleteReq;
		}
		if (_body.size() > CLIENT_MAX_BODY_SIZE) {
			_status = RequestStatus::ContentTooLarge;
			_keepAlive = false;
			_buffer.clear();
			return;
		}
	}
}

/**
 * If request has header Connection with either keep-alive or close,
 * _keepAlive will be set accordingly. If it has neither of those,
 * by default 1.1 request will keep connection alive, and 1.0 request won't.
 */
bool	Request::fillKeepAlive(void) {
	auto	it = _headers.find("connection");
	bool	hasClose = false;
	bool	hasKeepAlive = false;
	if (it != _headers.end()) {
		for (auto value = it->second.begin(); value != it->second.end(); value++)
		{
			if (*value == "close") {
				hasClose = true;
			}
			if (*value == "keep-alive") {
				hasKeepAlive = true;
			}
		}
		if (hasClose && hasKeepAlive)
			return false;
		else if (hasClose)
			_keepAlive = false;
		else if (hasKeepAlive)
			_keepAlive = true;
	}
	if (it == _headers.end() || (!hasClose && !hasKeepAlive)) {
		if (_request.httpVersion == "HTTP/1.1")
			_keepAlive = true;
		if (_request.httpVersion == "HTTP/1.0")
			_keepAlive = false;
	}
	return true;
}

/**
 * Checks if the headers include "host" (considered mandatory for 1.1), and if unique
 * headers only have one value each. Checks also for connection, to set keep-alive
 * flag if needed, and content-length, to set the length, and transfer-encoding for
 * chunked flag.
 */
bool	Request::validateHeaders(void) {
	auto	it = _headers.find("host");
	if (_request.httpVersion == "HTTP/1.1" && (it == _headers.end() || it->second.empty()))
		return false;
	for (auto const& [key, values] : _headers) {
		if (values.size() > 1 && isUniqueHeader(key))
			return false;
	}
	if (!fillKeepAlive())
		return false;
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
	if (it != _headers.end() && it->second.front() == "chunked") {
		if (_request.httpVersion == "HTTP/1.0")
			return false;
		_chunked = true;
	}
	it = _headers.find("content-type");
	if (it != _headers.end()) {
		if (std::find(it->second.begin(), it->second.end(), "multipart/form-data") != it->second.end()) {
			auto	value = it->second.begin();
			for (value = it->second.begin(); value != it->second.end(); value++) {
				if (value->substr(0, 9) == "boundary=") {
					_boundary = value->substr(9);
					break;
				}
			}
			if (value == it->second.end())
				return false;
		}
	}
	return true;
}

/**
 * Defines headers that can have only one value, and checks if any of them has more.
 */
bool	Request::isUniqueHeader(std::string const& key) {
	std::unordered_set<std::string>	uniques = {
		"access-control-request-method",
		"alt-used", // added
		"authorization",
		"content-length",
		"content-location", // added
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
		"sec-fetch-dest", // added
		"sec-fetch-mode", //added
		"sec-fetch-site", // added
		"sec-fetch-storage-access", // added
		"sec-fetch-user", // added
		"sec-purpose", // added
		"sec-websocket-key", // added
		"sec-websocket-version", // added
		"service-worker-header", // added
		"service-worker-navigation-preload", // added
		"upgrade-insecure-requests", // added
		"x-forwarded-host", // added
		"x-forwarded-proto", // added
	};
	auto	it = uniques.find(key);
	if (it != uniques.end())
		return true ;
	return false ;
}

/**
 * Validates target path characters.
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
 */
bool	Request::validateAndAssignTarget(std::string& target) {
	if (target.size() == 1 && target != "/")
		return false;
	if (!areValidChars(target))
		return false;
	size_t	protocolEnd = target.find("://");
	if (protocolEnd != std::string::npos)
	{
		std::string protocol = target.substr(0, protocolEnd);
		if (protocol != "http" && protocol != "https")
			return false;
	}
	size_t	queryStart = target.find('?');
	if (queryStart != std::string::npos)
	{
		_request.target = target.substr(0, queryStart);
		_request.query = target.substr(queryStart + 1);
	}
	else
		_request.target = target;
	return true;
}

/**
 * Only accepts HTTP/1.0 and HTTP/1.1 as valid versions on the request line.
 */
bool	Request::validateAndAssignHttp(std::string& httpVersion) {
	if (!std::regex_match(httpVersion, std::regex("HTTP/1.([01])")))
		return false ;
	_request.httpVersion = httpVersion;
	return true;
}

/**
 * Helper function to print RequestStatus value.
 */
static void	printStatus(RequestStatus status)
{
	switch (status)
	{
		case RequestStatus::WaitingData:
			std::cout << "Waiting for more data\n";
			break;
		case RequestStatus::CompleteReq:
			std::cout << "Complete and valid request received\n";
			break;
		case RequestStatus::ContentTooLarge:
			std::cout << "Content too large\n";
			break;
		case RequestStatus::Error:
			std::cout << "Critical error found, client to be disconnected\n";
			break;
		case RequestStatus::Invalid:
			std::cout << "Invalid HTTP request\n";
			break;
		case RequestStatus::ReadyForResponse:
			std::cout << "Ready to receive response\n";
			break;
		default:
			std::cout << "Unknown\n";
	}
}

/**
 * Prints parsed data for debugging.
 */
void	Request::printData(void) const
{
	std::cout << "\n---- Request line ----\nMethod: ";
	switch(_request.method) {
		case RequestMethod::Get:
			std::cout << "GET";
			break;
		case RequestMethod::Post:
			std::cout << "POST";
			break;
		case RequestMethod::Delete:
			std::cout << "DELETE";
			break;
		default:
			throw std::runtime_error("HTTP request method unknown\n");
	}

	std::cout << "\nTarget: " << _request.target << "\nHTTP version: " << _request.httpVersion << "\n";
	if (_request.query.has_value())
		std::cout << "Query: " << _request.query.value() << '\n';
	std::cout << "----------------------\n\n";

	std::cout << "---- Headers ----\n";
	for (auto it = _headers.begin(); it != _headers.end(); it++) {
		std::cout << it->first << ": ";
		for (auto value = it->second.begin(); value != it->second.end(); value++)
			std::cout << *value << " ";
		std::cout << '\n';
	}
	std::cout << "-----------------\n\n";

	if (!_body.empty())
		std::cout << "---- Body ----\n" << _body << "----------------\n\n";

	std::cout << "	Keep alive?		" << _keepAlive << '\n';
	std::cout << "	Complete headers?	" << _completeHeaders << '\n';
	std::cout << "	Status?			";
	printStatus(_status);
	std::cout << "	Chunked?		" << _chunked << "\n";
	if (_boundary.has_value())
		std::cout << "	Boundary:		'" << _boundary.value() << "'\n";
	std::cout << "\n";
}

void	Request::setIdleStart(void) {
	_idleStart = std::chrono::high_resolution_clock::now();
	DEBUG_LOG("Fd " + std::to_string(_fd) + " _idleStart set to " + std::to_string(_idleStart.time_since_epoch().count()));
}

void	Request::setRecvStart(void) {
	_recvStart = std::chrono::high_resolution_clock::now();
	DEBUG_LOG("Fd " + std::to_string(_fd) + " _recvStart set to " + std::to_string(_recvStart.time_since_epoch().count()));
}

void	Request::setSendStart(void) {
	_sendStart = std::chrono::high_resolution_clock::now();
	DEBUG_LOG("Fd " + std::to_string(_fd) + " _sendStart set to " + std::to_string(_sendStart.time_since_epoch().count()));
}

void	Request::resetSendStart(void) {
	_sendStart = {};
}

void	Request::resetBuffer(void) {
	if (!_buffer.empty()) {
		INFO_LOG("The remaining request data in buffer following one complete request will be discarded");
		_buffer.clear();
	}
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

RequestStatus	Request::getStatus(void) const {
	return _status;
}

void	Request::setStatus(RequestStatus status) {
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

std::vector<std::string> const	*Request::getHeader(std::string const &key) const
{
	try {
		return &_headers.at(key);
	} catch (std::exception const &e) {
		return nullptr;
	}
}

void	Request::setUploadFD(std::unique_ptr<std::ofstream> outfile) {
	_uploadFD = std::move(outfile);
}

std::ofstream*	Request::getUploadFD(void) {
	return (_uploadFD.get());
}

size_t	Request::getCurrentUploadPosition(void) {
	return (_curr_upload_pos);
}

void	Request::setCurrentUploadPosition(size_t pos) {
	_curr_upload_pos = pos;
}

bool	Request::isHeadersCompleted(void) const {
	return (_completeHeaders);
}

size_t	Request::getContentLength(void) const {
	if(_contentLen.has_value())
		return (_contentLen.value());
	return (0);
}

void	Request::setUploadDir(std::string path) {
	_uploadDir = path;
}

std::string	Request::getUploadDir(void) {
	return (_uploadDir);
}

void	Request::handleFileUpload(void) {

	std::string partDelimiter = "--" + _boundary.value();
	std::string endDelimiter =  partDelimiter + "--";

	size_t currPos = this->getCurrentUploadPosition();
	while (true) {
		size_t partStart = _buffer.find(partDelimiter, currPos);

		if(partStart == std::string::npos) {
			_status = RequestStatus::WaitingData;
			break;
		}

		if(_buffer.compare(partStart, endDelimiter.length(), endDelimiter) == 0) {
			_status = RequestStatus::CompleteReq;
			// Remove only the processed multipart data including end delimiter
			_buffer.erase(0, partStart + endDelimiter.length());
			// Skip trailing CRLF if present

			if (_buffer.size() >= 2 && _buffer.compare(0, 2, "\r\n") == 0) {
				_buffer = _buffer.erase(0, 2);
			}
			break;
		}

		size_t headerStart = partStart + partDelimiter.length();

		if(_buffer.substr(headerStart, 2) == "\r\n") {
			headerStart += 2;
		}

		size_t partEnd = _buffer.find(partDelimiter, headerStart);

		if(partEnd == std::string::npos) {
			_status = RequestStatus::WaitingData;
			break;
		}

		if ((partEnd - headerStart) < 2) {
			_status = RequestStatus::Error;
			_keepAlive = false;
			return;
		}

		std::string raw_part = _buffer.substr(headerStart, partEnd - (headerStart + 2));

		MultipartPart mp;
		size_t header_end = raw_part.find("\r\n\r\n");

		if(header_end != std::string::npos) {
			mp.headers = raw_part.substr(0, header_end);
			mp.data = raw_part.substr(header_end + 4);
			mp.name = extractQuotedValue(mp.headers, "name=");
			mp.filename = extractQuotedValue(mp.headers, "filename=");
			mp.contentType = extractValue(mp.headers, "Content-Type: ");
		}
		if(_uploadFD) {
			try {

				std::ofstream* ofs = this->getUploadFD();
				if(ofs) {
					saveToDisk(mp, *ofs);
				} else {
					ERROR_LOG("Upload FD became null unexpectedly");
					_status = RequestStatus::Error;
					return;
				}
			} catch (const std::exception& e) {
				ERROR_LOG("Failed to save upload part: " + std::string(e.what()));
				_uploadFD->close();
				_status = RequestStatus::Error;
				return;
			}
		} else {
			try {
				auto fd = initialSaveToDisk(mp, getUploadDir());
				if (!fd) {
					ERROR_LOG("Failed to initialize upload file descriptor");
					_status = RequestStatus::Error;
					return;
				}
				this->setUploadFD(std::move(fd));
			} catch (const std::exception& e) {
				ERROR_LOG("Failed to initialize upload: " + std::string(e.what()));
				_status = RequestStatus::Error;
				return;
			}
		}
		this->setCurrentUploadPosition(partEnd);
		currPos = partEnd;
	}
}
