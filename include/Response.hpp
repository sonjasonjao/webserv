#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include "Request.hpp"

enum ResponseCode : int {
	Unassigned	= -1,
	OK			= 200,
	NoContent	= 204,
	BadRequest	= 400,
	NotFound	= 404,
};

class Response {

public:
	Response() = delete;
	Response(Request const &req);
	Response(Response const &other);
	~Response() = default;

	std::string const								&getContent();
	std::optional<std::vector<std::string> const *>	getHeader(std::string const &key);
	RequestMethod									getRequestMethod();

private:

	using strVecMap	= std::unordered_map<std::string, std::vector<std::string> >;

	Request const	&_req;
	strVecMap		_headers;
	std::string		_startLine;
	std::string		_headerSection;
	std::string		_body;
	std::string		_content;
	ResponseCode	_code = Unassigned;
};
