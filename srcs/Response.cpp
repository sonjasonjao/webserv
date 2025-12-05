#include "Response.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

std::string	getImfFixdate();

constexpr char const * const	CRLF = "\r\n";

Response::Response(Request const &req) : _req(req)
{
	_startLine = req.getHttpVersion();

	_headerSection	 = std::string("Server: ") + req.getHost() + CRLF;
	_headerSection	+= "Date: " + getImfFixdate() + CRLF;

	_body = "<!DOCTYPE html><html><head></head><body>Placeholder page</body></html>";

	if (!req.getIsValid()) {
		// Why isn't it valid? -> Find out!

		// Assume bad request for now?
		_startLine	+= " 400 Bad Request";
		_statusCode	 = BadRequest;
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

	std::string	target = req.getTarget();

	if (std::filesystem::is_directory(target)) {
		if (target.back() == '/')
			target.pop_back();
		target = target + "/" + "index.html";	// NOTE: this could be toggled by an option in the config
	}

	// NOTE:	Replace all current body functionality with content getter functions
	//			that either read from a file or fetch a buffer from memory

	switch (req.getRequestMethod()) {
		case RequestMethod::Get:
			if (!resourceExists(target)) {
				INFO_LOG("Resource " + target + " could not be found");
				_startLine	+= " 404 Not Found";
				_statusCode	 = NotFound;
				_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
				_body		 = "<!DOCTYPE html><html><head></head><body>404: resource not found</body></html>";
				break;
			}
			_startLine		+= " 200 OK";
			_statusCode		 = OK;
			_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
			_body			 = getFileAsString(target);
		break;
		case RequestMethod::Post:
			_startLine		+= " 200 OK";
			_statusCode		 = OK;
			_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
			_body			 = "<!DOCTYPE html><html><head></head><body>200: POST OK page</body></html>";
		break;
		case RequestMethod::Delete:
			if (!resourceExists(target)) {
				INFO_LOG("Response: Resource " + target + " could not be found");
				_startLine	+= " 404 Not Found";
				_statusCode	 = NotFound;
				_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
				_body		 = "<!DOCTYPE html><html><head></head><body>404: resource not found</body></html>";
				break;
			}
			_startLine		+= "204 No Content";
			_statusCode		 = NoContent;
			_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
			_body			 = "<!DOCTYPE html><html><head></head><body>204: no content</body></html>";
		break;
		default:
			INFO_LOG("Bad request");
			_startLine	= " 400 Bad Request";
			_statusCode	= BadRequest;
			_headerSection 	+= std::string("Content-Type: text/html") + CRLF;
			_body		 = "<!DOCTYPE html><html><head></head><body>400: bad request</body></html>";
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
 */
std::vector<std::string> const *	Response::getHeader(std::string const &key)
{
	try {
		return &_headers.at(key);
	} catch (std::exception const &e) {
		return nullptr;
	}
}

RequestMethod	Response::getRequestMethod()
{
	return _req.getRequestMethod();
}
