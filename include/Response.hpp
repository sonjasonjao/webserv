#pragma once

#include <string>
#include "Request.hpp"
#include "Parser.hpp"

enum ResponseCode : int {
	Unassigned		= -1,
	OK				= 200,
	NoContent		= 204,
	BadRequest		= 400,
	Forbidden		= 403,
	NotFound		= 404,
	RequestTimeout	= 408,
	ContentTooLarge	= 413,
};

class Response {

public:
	Response() = delete;
	Response(Request const &req, Config const &conf);
	Response(Response const &other) = default;
	~Response() = default;

	std::string const	&getContent() const;
	int					getResponseCode() const;

	void	sendToClient();
	bool	sendIsComplete() const;

private:

	void	formResponse();

	Request const	&_req;
	Config const	&_conf;
	std::string		_target;
	std::string		_startLine;
	std::string		_headerSection;
	std::string		_body;
	std::string		_content;
	std::string		_contentType;
	ResponseCode	_statusCode	= Unassigned;
	size_t			_bytesSent	= 0;
};
