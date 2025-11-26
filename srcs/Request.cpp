#include "../include/Request.hpp"

Request::Request(std::string buf) : _isValid(true)
{
	std::string line, method, target, httpVersion;
	std::istringstream ss(buf);
	std::vector<std::string>	methods = { "GET", "POST", "DELETE "};
	getline(ss, line);
	std::istringstream req(line);
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
	printData();
}

bool	Request::isTargetValid(std::string& target)
{
	if (target[0] == '/')
	{
		_request.target = target;
		return true ;
	}
	return false ;
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
	std::cout << "Method: " << _request.method << ", target: " << _request.target << ", HTTP version: "
		<< _request.httpVersion << '\n';
}

bool	Request::isValid(void) const
{
	return _isValid;
}

Request::~Request(void)
{
}
