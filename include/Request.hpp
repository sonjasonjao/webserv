#pragma once
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <fcntl.h>
#include <unistd.h>
#include <optional>

enum method
{
	GET,
	POST,
	DELETE
};

struct requestLine
{
	std::string					target;
	std::optional<std::string>	query;
	std::string					httpVersion;
	enum method					method;
};

class Request
{
	using stringMap = std::unordered_map<std::string, std::string>;

	private:
		struct requestLine	_request;
		stringMap			_headers;
		std::string			_body;
		bool				_isValid;
		bool				_isMissingData;

	public:
		Request() = delete;
		Request(std::string buf);
		~Request();

		void	parseRequestLine(std::istringstream& req);
		void	parseHeaders(std::istringstream& ss);
		void	printData(void) const;
		bool	isTargetValid(std::string& target);
		bool	areValidChars(std::string& target);
		bool	isHttpValid(std::string& httpVersion);
		bool	isValid(void) const;
};
