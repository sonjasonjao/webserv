#include "Request.hpp"
#include "Log.hpp"
#include "Response.hpp"
#include "Utils.hpp"
#include <regex>
#include <iostream>
#include <unordered_set>
#include <chrono>
#include <filesystem>

constexpr char const * const	CRLF = "\r\n";

/**
 * Initializes attribute values for request. Http version is now by default initialized to HTTP/1.1,
 * so if the http version given in the request is invalid, 1.1 will be used to send the error page
 * response.
 */
Request::Request(int fd, int serverFd)
	:	_fd(fd),
		_serverFd(serverFd),
		_keepAlive(false),
		_chunked(false),
		_completeHeaders(false),
		_uploadFD(nullptr),
		_headerSize(0),
		_currUploadPos(0)
{
	_request.method = RequestMethod::Unknown;
	_status = ClientStatus::WaitingData;
	_responseCodeBypass = Unassigned;
	_idleStart = std::chrono::high_resolution_clock::now();
	_recvStart = {};
	_sendStart = {};
	_request.httpVersion = "HTTP/1.1";
}

/**
 * Saves the current buffer filled by recv into the combined buffer of this client.
 * Checks whether the buffer so far includes "\r\n\r\n". If not, and the headers section hasn't
 * been received completely (ending with "\r\n\r\n"), we assume the request is partial.
 */
void	Request::processRequest(std::string const& buf)
{
	_buffer += buf;
	if (_buffer.find("\r\n\r\n") == std::string::npos && !_completeHeaders) {
		_status = ClientStatus::WaitingData;
	}
	else
		parseRequest();
}

/**
 * After receiving and parsing a complete request, and handling it (= building a response and
 * sending it), these properties of the current client are reset for a possible following request.
 */
void	Request::reset()
{
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
	_currUploadPos = 0;
	if (_uploadFD) {
		_uploadFD->close();
		_uploadFD.reset();
	}
	_boundary.reset();
}

/**
 * Resets the keepAlive status separately from other resets, only after keepAlive status of the
 * latest request has been checked.
 */
void	Request::resetKeepAlive()
{
	_keepAlive = false;
}

/**
 * In handleConnections, each client fd is checked for possible timeouts by comparing
 * the stored _idleStart, _recvStart, and _sendStart with the current time stamp. Helper
 * variable init is used to check whether _recvStart or _sendStart has ever been updated
 * after the initialization to zero.
 */
void	Request::checkReqTimeouts()
{
	auto		now = std::chrono::high_resolution_clock::now();
	auto		diff = now - _idleStart;
	auto		durMs = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
	timePoint	init = {};
	if (durMs.count() > IDLE_TIMEOUT) {
		_status = ClientStatus::IdleTimeout;
		DEBUG_LOG("Idle timeout with client fd " + std::to_string(_fd));
		_keepAlive = false;
		return;
	}
	diff = now - _recvStart;
	durMs = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
	if (_recvStart != init && durMs.count() > RECV_TIMEOUT) {
		_status = ClientStatus::RecvTimeout;
		DEBUG_LOG("Recv timeout with client fd " + std::to_string(_fd));
		_keepAlive = false;
		return;
	}
	diff = now - _sendStart;
	durMs = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
	if (_sendStart != init && durMs.count() > SEND_TIMEOUT) {
		_status = ClientStatus::SendTimeout;
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
	auto		it	= orig.find(delim);
	std::string	tmp	= "";

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
void	Request::parseRequest()
{
	if (_request.method == RequestMethod::Unknown) {
		std::string	reqLine = extractFromLine(_buffer, CRLF);

		parseRequestLine(reqLine);
		if (_status == ClientStatus::Invalid) {
			_buffer.clear();
			return;
		}
	}
	if (!_completeHeaders)
		parseHeaders(_buffer);
	if (_status == ClientStatus::Invalid || _status == ClientStatus::Error) {
		_buffer.clear();
		return;
	}

	if (!_contentLen.has_value() && !_chunked)
		_status = ClientStatus::CompleteReq;
	else if (_request.method == RequestMethod::Post && _boundary.has_value()) {
		return;
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
			_responseCodeBypass = ContentTooLarge;
			_status = ClientStatus::Invalid;
			_buffer.clear();
			return;
		}
		if (_body.size() < _contentLen.value())
			_status = ClientStatus::WaitingData;
		else if (_body.size() == _contentLen.value())
			_status = ClientStatus::CompleteReq;
	}
	else if (_chunked)
		parseChunked();
	printData();
}

/**
 * Splits the request line into tokens, recognises method, and validates target path
 * and HTTP version.
 */
void	Request::parseRequestLine(std::string &req)
{
	std::string method, target, httpVersion;
	std::vector<std::string>	methods = { "GET", "POST", "DELETE" };
	method = extractFromLine(req, " ");
	target = extractFromLine(req, " ");
	httpVersion = req;
	if (method.empty() || target.empty() || httpVersion.empty()) {
		_status = ClientStatus::Invalid;
		return;
	}
	size_t	i = 0;
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
			_status = ClientStatus::Invalid;
			return;
	}
	if (!validateAndAssignTarget(target) || !validateAndAssignHttp(httpVersion))
	{
		_status = ClientStatus::Invalid;
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
void	Request::parseHeaders(std::string& str)
{
	std::string	line;
	size_t	headEnd = str.find("\r\n\r\n");
	if (headEnd == std::string::npos)
		headEnd = str.size();
	_headerSize = headEnd;
	if (_headerSize > HEADERS_MAX_SIZE) {
		_status = ClientStatus::Invalid;
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
			_status = ClientStatus::Error;
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
			_status = ClientStatus::Invalid;
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
		_status = ClientStatus::WaitingData;
	if (_headers.empty() || !validateHeaders()) {
		_status = ClientStatus::Invalid;
		_keepAlive = false;
		return;
	}
}

/**
 * In the case of a chunked request, attempts to check the size of each chunk and split the
 * string accordingly to store that chunk into body.
 */
void	Request::parseChunked()
{
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
					_status = ClientStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = extractFromLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_status = ClientStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_status = ClientStatus::WaitingData;
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
					_status = ClientStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_buffer = _buffer.substr(pos + 2);
				std::string	tmp = extractFromLine(_buffer, CRLF);
				if (tmp.size() != len) {
					_status = ClientStatus::Invalid;
					_keepAlive = false;
					_buffer.clear();
					return;
				}
				_body += tmp;
				pos = _buffer.find(CRLF);
			}
			_body += extractFromLine(_buffer, "0\r\n\r\n");
			_status = ClientStatus::CompleteReq;
		}
		if (_body.size() > CLIENT_MAX_BODY_SIZE) {
			_responseCodeBypass = ContentTooLarge;
			_status = ClientStatus::Invalid;
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
bool	Request::fillKeepAlive()
{
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

bool	Request::boundaryHasValue()
{
	return _boundary.has_value();
}

/**
 * Checks if the headers include "host" (considered mandatory for 1.1), and if unique
 * headers only have one value each. Checks also for connection, to set keep-alive
 * flag if needed, and content-length, to set the length, and transfer-encoding for
 * chunked flag.
 */
bool	Request::validateHeaders()
{
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
bool	Request::isUniqueHeader(std::string const& key)
{
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
bool	Request::areValidChars(std::string& s)
{
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
bool	Request::validateAndAssignTarget(std::string& target)
{
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
bool	Request::validateAndAssignHttp(std::string& httpVersion)
{
	if (!std::regex_match(httpVersion, std::regex("HTTP/1.([01])")))
		return false ;
	_request.httpVersion = httpVersion;
	return true;
}

/**
 * Helper function to print ClientStatus value.
 */
static void	printStatus(ClientStatus status)
{
	switch (status)
	{
		case ClientStatus::WaitingData:
			std::cout << "Waiting for more data\n";
			break;
		case ClientStatus::CompleteReq:
			std::cout << "Complete and valid request received\n";
			break;
		case ClientStatus::Error:
			std::cout << "Critical error found, client to be disconnected\n";
			break;
		case ClientStatus::Invalid:
			std::cout << "Invalid HTTP request\n";
			break;
		case ClientStatus::ReadyForResponse:
			std::cout << "Ready to receive response\n";
			break;
		default:
			std::cout << "Unknown\n";
	}
}

/**
 * Prints parsed data for debugging.
 */
void	Request::printData() const
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

void	Request::setIdleStart()
{
	_idleStart = std::chrono::high_resolution_clock::now();
	DEBUG_LOG("Fd " + std::to_string(_fd) + " _idleStart set to " + std::to_string(_idleStart.time_since_epoch().count()));
}

void	Request::setRecvStart()
{
	_recvStart = std::chrono::high_resolution_clock::now();
	DEBUG_LOG("Fd " + std::to_string(_fd) + " _recvStart set to " + std::to_string(_recvStart.time_since_epoch().count()));
}

void	Request::setSendStart()
{
	_sendStart = std::chrono::high_resolution_clock::now();
	DEBUG_LOG("Fd " + std::to_string(_fd) + " _sendStart set to " + std::to_string(_sendStart.time_since_epoch().count()));
}

void	Request::resetSendStart()
{
	_sendStart = {};
}

void	Request::resetBuffer()
{
	if (!_buffer.empty()) {
		DEBUG_LOG("Discarding remaining data in buffer:\n'" + _buffer + "'");
		_buffer.clear();
	}
}

std::string	Request::getHost() const
{
	std::string	host;

	try {
		host = (_headers.at("host")).front();
	} catch(const std::exception& e) {}

	return host;
}

int	Request::getFd() const
{
	return _fd;
}

int	Request::getServerFd() const
{
	return _serverFd;
}

bool	Request::getKeepAlive() const
{
	return _keepAlive;
}

bool	Request::isHeadersCompleted() const
{
	return _completeHeaders;
}

ClientStatus	Request::getStatus() const
{
	return _status;
}

void	Request::setStatus(ClientStatus status)
{
	_status = status;
}

void	Request::setResponseCodeBypass(ResponseCode code)
{
	_responseCodeBypass = code;
}

std::string const	&Request::getBuffer() const
{
	return _buffer;
}

RequestMethod	Request::getRequestMethod() const
{
	return _request.method;
}

ResponseCode	Request::getResponseCodeBypass() const
{
	return _responseCodeBypass;
}

std::string const	&Request::getHttpVersion() const
{
	return _request.httpVersion;
}

std::string const	&Request::getBody() const
{
	return _body;
}

std::string const	&Request::getTarget() const
{
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

size_t	Request::getContentLength() const
{
	if (_contentLen.has_value())
		return _contentLen.value();
	return 0;
}

void	Request::setUploadDir(std::string path)
{
	_uploadDir = path;
}

void	Request::handleFileUpload()
{
	std::string	partDelimiter	= "--" + _boundary.value();
	std::string	endDelimiter	= partDelimiter + "--";
	size_t		currPos			= _currUploadPos;

	while (true) {
		size_t partStart = _buffer.find(partDelimiter, currPos);

		if (partStart == std::string::npos) {
			_status = ClientStatus::WaitingData;
			break;
		}

		if (_buffer.compare(partStart, endDelimiter.length(), endDelimiter) == 0) {
			// Remove only the processed multipart data including end delimiter
			_buffer.erase(0, partStart + endDelimiter.length());
			// Skip trailing CRLF if present
			if (_buffer.length() >= 2 && _buffer.find(CRLF) == 0)
				_buffer.erase(0, 2);

			/*NOTE! This if condition is not working as intended now. In a valid
			case, this gets triggered with some trailing newline.*/
			if (!_buffer.empty()) {
				DEBUG_LOG("Expected empty buffer after removing end delimiter, remainder: " + _buffer);
				_responseCodeBypass = BadRequest;
				_status = ClientStatus::Invalid;
				_keepAlive = false;
				break;
			}
			_responseCodeBypass = Created;
			_status = ClientStatus::CompleteReq;
			break;
		}

		size_t	headerStart = partStart + partDelimiter.length();

		if (_buffer.substr(headerStart, 2) == "\r\n") {
			headerStart += 2;
		}

		size_t	partEnd = _buffer.find(partDelimiter, headerStart);

		if (partEnd == std::string::npos) {
			_status = ClientStatus::WaitingData;
			break;
		}

		if ((partEnd - headerStart) < 2) {
			_status = ClientStatus::Error;
			_keepAlive = false;
			return;
		}

		std::string	raw_part = _buffer.substr(headerStart, partEnd - (headerStart + 2));

		MultipartPart mp;
		size_t header_end = raw_part.find("\r\n\r\n");

		if (header_end != std::string::npos) {
			mp.headers = raw_part.substr(0, header_end);
			mp.data = raw_part.substr(header_end + 4);
			mp.name = extractQuotedValue(mp.headers, "name=");
			mp.filename = extractQuotedValue(mp.headers, "filename=");
			mp.contentType = extractValue(mp.headers, "Content-Type: ");
		}

		if (_uploadFD) {
			if (!saveToDisk(mp))
				break;
		} else {
			if (!initialSaveToDisk(mp))
				break;
		}

		_currUploadPos = partEnd;
		currPos = partEnd;
	}
}

bool	Request::saveToDisk(const MultipartPart& part)
{
	_uploadFD->write(part.data.c_str(), part.data.size());

	if (!_uploadFD->good()) {
		ERROR_LOG("Writing data to the file failed");
		_responseCodeBypass = InternalServerError;
		_status = ClientStatus::Invalid;
		return false;
	}

	DEBUG_LOG("File " + part.filename + " saved successfully!");
	return true;
}

bool	Request::initialSaveToDisk(const MultipartPart& part)
{
	// if the upload directory has not set in the config file upload operation is forbidden
	if(!_uploadDir.has_value()) {
		ERROR_LOG("Upload directory has not set in the config file");
		_responseCodeBypass = Forbidden;
		_status = ClientStatus::Invalid;
		return false;
	}

	try {
		// will create if not exists
		if (!std::filesystem::exists(_uploadDir.value()))
			std::filesystem::create_directories(_uploadDir.value());

	} catch (const std::filesystem::filesystem_error& e) {
		ERROR_LOG("Failed to create upload directory: " + std::string(e.what()));
		_responseCodeBypass = InternalServerError;
		_status = ClientStatus::Invalid;
		return false;
	}

	// constructing the file path
	std::filesystem::path	target_path = std::filesystem::path(_uploadDir.value()) / std::filesystem::path(part.filename).filename();

	// if filename conflicts will treat as an error
	if (std::filesystem::exists(target_path)) {
		ERROR_LOG("File '" + std::string(target_path) + "' already exists");
		_responseCodeBypass = Conflict;
		_status = ClientStatus::Invalid;
		return false;
	}

	// file handler to write data
	_uploadFD = std::make_unique<std::ofstream>(target_path, std::ios::binary);

	if (_uploadFD && _uploadFD->is_open()) {
		_uploadFD->write(part.data.c_str(), part.data.size());
		if (_uploadFD->good()) {
			DEBUG_LOG("File " + part.filename + " initial write successful!");
			return true;
		} else {
			ERROR_LOG("File " + part.filename + " initial write failed!");
			_uploadFD->close();
			_responseCodeBypass = InternalServerError;
			_status = ClientStatus::Invalid;
			return false;
		}
	} else {
		ERROR_LOG("File " + part.filename + " save process failed!");
		_responseCodeBypass = InternalServerError;
		_status = ClientStatus::Invalid;
		return false;
	}
}
