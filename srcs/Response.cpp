#include "Response.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include "Pages.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

constexpr char const * const	CRLF = "\r\n";

Response::Response(Request const &req) : _req(req)
{
	static std::string	currentDir = std::filesystem::current_path();

	if (!req.getIsValid()) {
		// Why isn't it valid? -> Find out!

		// Assume bad request for now?
		_statusCode	 = BadRequest;
		_body		 = Pages::getPageContent("default400");

		formResponse();

		return;
	}

	// Validate request
	//	date, fields, lengths, delimiters

	// Match request type
	//	if ok
	//		get correct config
	//		get correct content
	//			from file or from memory?
	//			What if the content is huge? Chunking time?

	_target = req.getTarget();

	if (!uriFormatOk(_target) || uriTargetAboveRoot(_target)) {
		_error		= ResponseError::badTarget;
		_statusCode	= BadRequest;
		_body		= Pages::getPageContent("default400");

		formResponse();

		return;
	}

	if (std::filesystem::is_directory(_target)) {
		if (_target.back() == '/')
			_target.pop_back();
		_target = _target + "/" + "index.html";	// NOTE: this could be toggled by an option in the config
	}

	// NOTE:	Replace all current body functionality with content getter functions
	//			that either read from a file or fetch a buffer from memory

	switch (req.getRequestMethod()) {
		case RequestMethod::Get:
			if (!Pages::isCached(getAbsPath(_target)) && !resourceExists(_target, currentDir)) {
				INFO_LOG("Resource " + _target + " could not be found");
				_statusCode = NotFound;
				break;
			}
			INFO_LOG("Resource " + _target + " found");
			_statusCode = OK;
		break;
		case RequestMethod::Post:
			INFO_LOG("Responding to POST request with target " + _target);
			_statusCode = OK;
		break;
		case RequestMethod::Delete:
			if (!resourceExists(_target, currentDir)) {
				INFO_LOG("Response: Resource " + _target + " could not be found");
				_statusCode		 = NotFound;
				break;
			}
			INFO_LOG("Resource " + _target + " deleted (not really but in the future)");
			_statusCode		 = NoContent;
		break;
		default:
			INFO_LOG("Bad request");
			_statusCode		 = BadRequest;
		break;
	}

	formResponse();

	std::cout << "\n---- Response content ----\n" << _content << "\n";
}

Response::Response(Response const &other)
	:	_req(other._req),
		_headers(other._headers),
		_target(other._target),
		_startLine(other._startLine),
		_headerSection(other._headerSection),
		_body(other._body),
		_content(other._content),
		_statusCode(other._statusCode)
{}

std::string const	&Response::getContent() const
{
	return _content;
}

std::string const	&Response::getStartLine() const
{
	return _startLine;
}

/**
 */
std::vector<std::string> const	*Response::getHeader(std::string const &key) const
{
	try {
		return &_headers.at(key);
	} catch (std::exception const &e) {
		return nullptr;
	}
}

RequestMethod	Response::getRequestMethod() const
{
	return _req.getRequestMethod();
}

void	Response::formResponse()
{
	_headerSection =	std::string("Server: ") + _req.getHost() + CRLF;
	_headerSection +=	"Date: " + getImfFixdate() + CRLF;
	_headerSection +=	"Content-Type: text/html" + std::string(CRLF);

	switch (_statusCode) {
		case 200:
			_startLine	= _req.getHttpVersion() + " 200 OK";
			if (!_target.empty())
				_body	= Pages::getPageContent(getAbsPath(_target));
			else
				_body	= Pages::getPageContent("default200");
		break;
		case 204:
			_startLine	= _req.getHttpVersion() + " 204 No Content";
			_body		= Pages::getPageContent("default204");
		break;
		case 400:
			_startLine	= _req.getHttpVersion() + " 400 Bad Request";
			_body		= Pages::getPageContent("default400");
		break;
		case 404:
			_startLine	= _req.getHttpVersion() + " 404 Not Found";
			_body		= Pages::getPageContent("default404");
		break;
		default:
			_startLine = _req.getHttpVersion() + " 400 Bad Request";
			_body		= Pages::getPageContent("default400");
	}

	_headerSection += "Content-Length: " + std::to_string(_body.length()) + CRLF;

	_content = _startLine + CRLF + _headerSection + CRLF + _body;
}
