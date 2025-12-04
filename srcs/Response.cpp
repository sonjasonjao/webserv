#include "Response.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include <optional>
#include <string>
#include <vector>
#include <iostream>

std::string	getImfFixdate();

constexpr char const * const	CRLF = "\r\n";

Response::Response(Request const &req) : _req(req)
{
	_startLine = req.getHttpVersion();

	_headerSection	 = std::string("Server: ") + req.getHost() + CRLF;
	_headerSection	+= "Date: " + getImfFixdate() + CRLF;

	_body = "<DOCTYPE! html><html><head></head><body>Placeholder page</body></html>";

	if (!req.getIsValid()) {
		// Why isn't it valid? -> Find out!

		// Assume bad request for now?
		_startLine	+= " 400 Bad Request";
		_code		 = BadRequest;
		_body		 = "";
	}

	// Validate request
	//	date, fields, lengths, delimiters

	// Match request type
	//	if ok
	//		get correct config
	//		get correct content
	//			from file or from memory?
	//			What if the content is huge? Chunking time?

	// NOTE:	Replace all current body functionality with content getter functions
	//			that either read from a file or fetch a buffer from memory
	switch (req.getRequestMethod()) {
		case RequestMethod::Get:
			if (!resourceExists(req.getTarget())) {
				INFO_LOG("Resource " + req.getTarget() + " could not be found");
				_startLine	+= " 404 Not Found";
				_code		 = NotFound;
				_body		 = "<DOCTYPE! html><html><head></head><body>404: resource not found</body></html>";
				break;
			}
			_startLine		+= " 200 OK";
			_code			 = OK;
			_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
			_body			 = getFileAsString(req.getTarget());
		break;
		case RequestMethod::Post:
			_startLine		+= " 200 OK";
			_code			 = OK;
			_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
			_body			 = "<DOCTYPE! html><html><head></head><body>200: POST OK page</body></html>";
		break;
		case RequestMethod::Delete:
			if (!resourceExists(req.getTarget())) {
				INFO_LOG("Response: Resource " + req.getTarget() + " could not be found");
				_startLine	+= " 404 Not Found";
				_code		 = NotFound;
				_body		 = "<DOCTYPE! html><html><head></head><body>404: resource not found</body></html>";
				break;
			}
			_startLine		+= "204 No Content";
			_code			 = NoContent;
			_body			 = "<DOCTYPE! html><html><head></head><body>204: no content</body></html>";
		break;
		default:
			_startLine	= " 400 Bad Request";
			_code		= BadRequest;
			_body		 = "<DOCTYPE! html><html><head></head><body>400: bad request</body></html>";
		break;
	}
	_headerSection 	+= "Content-Length: " + std::to_string(_body.length()) + CRLF;

	_content = _startLine + CRLF + _headerSection + CRLF + _body + CRLF;

	std::cout << "\nResponse content:\n" << _content << "\n";
}

Response::Response(Response const &other) : _req(other._req) {}

std::string const	&Response::getContent()
{
	return _content;
}

/**
 * NOTE:	The const pointer return type is a compromise, because std::optional
 *			doesn't support references. Of course it would be possible to create
 *			a new vector, but that would create duplicates in memory. The current
 *			way does also create the issue of a possible use after destroy though
 *			if the pointer stays in scope for longer than the underlying object.
 */
std::optional<std::vector<std::string> const *>	Response::getHeader(std::string const &key)
{
	try {
		auto	value = _headers.at(key);

		return &value;
	} catch (std::exception const &e) {
		return std::nullopt;
	}
}

RequestMethod	Response::getRequestMethod()
{
	return _req.getRequestMethod();
}
