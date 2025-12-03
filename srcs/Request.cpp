#include "../include/Request.hpp"

Request::Request(int fd) : _fd(fd), _keepAlive(false), _chunked(false), _isValid(true),
	_isMissingData(false) {
}

/**
 * Saves the current buffer filled by recv into the combined buffer of this client.
 *
 * Commented out earlier check if the request is now complete or still missing
 * something. That only checked if there is "\r\n\r\n" present in the buffer
 * (end of headers), but need to also account for excess data after headers, if
 * request has no body (no content-length or transfer-encodind: chunked header) or
 * in general if request does not end with \r\n. Might need to add a flag for whether
 * header is already completely received (= only possible body missing).
 */
void	Request::saveDataRequest(std::string buf) {
	_buffer += buf;
	_isMissingData = false;
	// size_t	end = _buffer.find("\r\n\r\n");
	// if (end == std::string::npos)
	// 	_isMissingData = true;
	// else
	// 	_isMissingData = false;
}

/**
 * Helper to split the buffer with delimiter ("\r\n"). Returns the current line until
 * the delimiter and updates _buffer by removing that extracted string.
 */
std::string	splitReqLine(std::string& orig, std::string delim)
{
	auto it = orig.find_first_of(delim);
	std::string tmp;
	if (it != std::string::npos)
	{
		tmp = orig.substr(0, it);
		if (orig.size() > it + 2)
			orig = orig.substr(it + 2);
		else
			orig.erase();
	}
	return tmp;
}

/**
 * Accepts as headers every line with ':' and stores each header as key and value to
 * an unordered map.
 *
 * Now requires only Host header as mandatory (requirement for HTTP/1.1). Need to check
 * if we must require others. HTTP/1.0 does not require host either?
 *
 * Which headers do we actually use? Right now, it splits header values by comma, but
 * some of them actually are separated e.g. by semicolon. Do we have to differentiate
 * these, or do we just have special handling for those we need to use (if any?)?
 */
void	Request::parseHeaders(std::string& str) {
	std::string	line;
	while (!str.empty()) {
		line = splitReqLine(str, "\r\n");
		const size_t point = line.find_first_of(":");
		if (point != std::string::npos) {
			std::string	key = line.substr(0, point);
			for (size_t i = 0; i < key.size(); i++)
				key[i] = std::tolower((unsigned char)key[i]);
			std::string value = line.substr(point + 2, line.size() - (point + 2));
			if (value.find(",") == std::string::npos)
				_headers[key].push_back(value);
			else {
				std::istringstream	values(value);
				std::string	oneValue;
				while (getline(values, oneValue, ',')) {
					_headers[key].push_back(oneValue);
				}
			}
		}
		else
			break ;
	}
	if (_headers.empty()) {
		_isValid = false;
		return ;
	}
	auto	it = _headers.find("host");
	if (it == _headers.end() || it->second.empty())
		_isValid = false;
	for (auto const& [key, values] : _headers) {
		if (values.size() > 1 && isUniqueHeader(key)) {
			_isValid = false;
			return ;
		}
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
}

/**
 * Validates and parses different sections of the request. After the last valid
 * header line, all possibly remaining data will for now be stored in one body string.
 *
 * What if body ends up exceeding content-length?
 */
void	Request::parseRequest(void) {
	if (_request.target.empty()) {
		std::string	reqLine = splitReqLine(_buffer, "\r\n");
		std::istringstream	req(reqLine);
		parseRequestLine(req);
		if (!_isValid)
			return ;
	}
	if (_headers.empty())
		parseHeaders(_buffer);
	if (!_isValid)
		return ;
	if (!_buffer.empty() && ((_contentLen.has_value() && _body.size() < _contentLen.value())
		|| _chunked)) {
		_body += _buffer;
	}
	if (_contentLen.has_value() && _body.size() < _contentLen.value())
		_isMissingData = true;
	if (_contentLen.has_value() && _body.size() == _contentLen.value())
		_isMissingData = false;
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
		if (s[i] < 32 || s[i] == 127 || s[i] == '<' || s[i] == '>'
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
	size_t	queryStart = target.find_first_of('?');
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

Request::~Request(void) {}
