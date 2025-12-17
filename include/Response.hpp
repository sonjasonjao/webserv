#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include "Request.hpp"

enum ResponseCode : int {
	Unassigned	= -1,
	OK			= 200,
	NoContent	= 204,
	BadRequest	= 400,
	NotFound	= 404,
};

enum class ResponseError {
	NoError,
	badTarget,
};

class Response {

public:
	Response() = delete;
	Response(Request const &req);
	Response(Response const &other);
	~Response() = default;

	std::string const				&getContent() const;
	std::string const				&getStartLine() const;
	std::vector<std::string> const	*getHeader(std::string const &key) const;
	RequestMethod					getRequestMethod() const;

private:

	using strVecMap	= std::unordered_map<std::string, std::vector<std::string> >;

	void	formResponse();

	Request const	&_req;
	strVecMap		_headers;
	std::string		_target;
	std::string		_startLine;
	std::string		_headerSection;
	std::string		_body;
	std::string		_content;
	ResponseCode	_statusCode	= Unassigned;
	ResponseError	_error		= ResponseError::NoError;
};
