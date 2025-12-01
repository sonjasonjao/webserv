#include "../include/Request.hpp"

/**
 * First checks if received request is complete or partial (need to figure out the best
 * way to handle that, now just a flag attribute). Validates and parses different
 * sections of the request. After the last valid header line, all possibly remaining
 * data will be stored in one body string.
 */
Request::Request(int fd, std::string buf) : _fd(fd), _keepAlive(false), _isValid(true),
	_isMissingData(false) {
	size_t	end = buf.find_last_of("\r\n");
	if (buf[end + 1] || end == std::string::npos)
		_isMissingData = true ;
	std::string			line;
	std::istringstream	ss(buf);
	getline(ss, line);
	std::istringstream	req(line);
	parseRequestLine(req);
	if (!_isValid)
		return ;
	parseHeaders(ss);
	if (!_isValid)
		return ;
	while (getline(ss, line))
		_body += line;
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
 * Accepts as headers every line with ':' and stores each header as key and value to
 * an unorderep map.
 *
 * Now requires only Host header as mandatory (requirement for HTTP/1.1). Need to check
 * if we must require others. HTTP/1.0 does not require host either?
 */
void	Request::parseHeaders(std::istringstream& ss) {
	std::string	line;
	while (true) {
		getline(ss, line);
		const size_t point = line.find_first_of(":");
		if (point != std::string::npos) {
			std::string	key = line.substr(0, point);
			for (size_t i = 0; i < key.size(); i++)
				key[i] = std::tolower((unsigned char)key[i]);
			std::string value = line.substr(point + 2);
			if (value.find(",") == std::string::npos) {
				_headers[key].push_back(value);
			}
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
			if (*con == "keep-alive\r") {
				_keepAlive = true;
				break ;
			}
		}
	}
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

void	Request::printData(void) const {
	std::cout << "----Request line:----\nMethod: ";
	switch(_request.method) {
		case RequestMethod::Get:
			std::cout << "Get\n";
			break ;
		case RequestMethod::Post:
			std::cout << "Post\n";
			break ;
		case RequestMethod::Delete:
			std::cout << "Delete\n";
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
