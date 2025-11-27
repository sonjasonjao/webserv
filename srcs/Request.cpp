#include "../include/Request.hpp"

Request::Request(std::string buf) : _isValid(true), _isMissingData(false)
{
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

void	Request::parseRequestLine(std::istringstream& req)
{
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
			_request.method = GET;
			break ;
		case 1:
			_request.method = POST;
			break ;
		case 2:
			_request.method = DELETE;
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

void	Request::parseHeaders(std::istringstream& ss)
{
	std::string	line;
	while (true)
	{
		getline(ss, line);
		const size_t point = line.find_first_of(":");
		if (point != std::string::npos)
			_headers[line.substr(0, point)] = line.substr(point + 2);
		else
			break ;
	}
	if (_headers.empty())
	{
		_isValid = false;
		return ;
	}
	//check for mandatory headers
}

bool	Request::areValidChars(std::string& s)
{
	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] < 32 || s[i] == 127 || s[i] == '<' || s[i] == '>'
			|| s[i] == '"' || s[i] == '\\')
			return false ;
	}
	return true ;
}

/**
 * If we don't handle OPTION method, will have to change not to accept a solo '*'
 */
bool	Request::isTargetValid(std::string& target)
{
	if (target.size() == 1 && target != "/" && target != "*")
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

bool	Request::isHttpValid(std::string& httpVersion)
{
	if (!std::regex_match(httpVersion, std::regex("HTTP/1.([01])")))
		return false ;
	_request.httpVersion = httpVersion;
	return true ;
}

void	Request::printData(void) const
{
	std::cout << "----Request line:----\nMethod: " << _request.method << ", target: "
		<< _request.target << ", HTTP version: " << _request.httpVersion << '\n';
	if (_request.query.has_value())
		std::cout << "Query: " << _request.query.value() << '\n';
	std::cout << "----Headers----:\n";
	for (auto it = _headers.begin(); it != _headers.end(); it++)
		std::cout << it->first << ": " << it->second << '\n';
	if (!_body.empty())
		std::cout << "----Body:----\n" << _body << '\n';
}

bool	Request::isValid(void) const
{
	return _isValid;
}

Request::~Request(void)
{
}
