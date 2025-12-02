#pragma once
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <regex>
#include <fcntl.h>
#include <unistd.h>
#include <optional>
#include <unordered_set>
#include "../include/Log.hpp"

/**
 * Mandatory methods required in the subject, do we want to add more? -> Will affect
 * request parsing and possibly class member attributes.
 */
enum class RequestMethod
{
	Get,
	Post,
	Delete
};

struct RequestLine
{
	std::string					target;
	std::optional<std::string>	query;
	std::string					httpVersion;
	RequestMethod				method;
};

class Request
{
	using stringMap = std::unordered_map<std::string, std::vector<std::string>>;

	private:
		int					_fd;
		std::string			_buffer;
		struct RequestLine	_request;
		stringMap			_headers;
		std::string			_body;
		bool				_keepAlive;
		bool				_isValid;
		bool				_isMissingData;

	public:
		Request() = delete;
		Request(int fd);
		~Request();

		void			saveDataRequest(std::string buf);
		void			parseRequest(void);
		void			parseRequestLine(std::istringstream& req);
		void			parseHeaders(std::istringstream& ss);
		void			printData(void) const;
		bool			isUniqueHeader(std::string const& key);
		bool			isTargetValid(std::string& target);
		bool			areValidChars(std::string& target);
		bool			isHttpValid(std::string& httpVersion);
		std::string		getHost(void);
		int				getFd(void) const;
		bool			getKeepAlive(void) const;
		bool			getIsValid(void) const;
		bool			getIsMissingData(void) const;
};
