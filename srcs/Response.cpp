#include "Response.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include "Pages.hpp"
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <sys/socket.h>
#include <unistd.h>

constexpr char const * const	CRLF = "\r\n";

/**
 * NOTE: Assumes that the programmer is using correct error page number strings as keys
 */
static std::string const	&getErrorPageContent(std::string const &key, Config const &conf)
{
	// First look in error_pages container in conf
	{
		auto	it = conf.error_pages.find(key);

		if (it != conf.error_pages.end() && resourceExists(it->second))
			return Pages::getPageContent(it->second);
	}
	// Second look in routes
	{
		auto	it = conf.routes.find(key);

		if (it != conf.routes.end() && resourceExists(it->second))
			return Pages::getPageContent(it->second);
	}

	// Retrieve default
	return Pages::getPageContent("default" + key);
}


static std::string	route(std::string target, Config const &conf)
{
	// Check for an exact route for target
	auto	it = conf.routes.find(target);

	if (it != conf.routes.end())
		return it->second;

	// Check for a partial route for target
	for (auto const &[key, val] : conf.routes) {
		if (key == "/")
			continue;

		auto	pos = target.find(key);

		if (pos != std::string::npos) {
			target.replace(pos, key.length(), val);
			return target;
		}
	}

	// If a route for "/" exists, add the value to the beginning of the target
	it = conf.routes.find("/");

	if (it != conf.routes.end()) {
		std::string	route = it->second;

		if (target[0] == '/')
			target = target.substr(1);
		if (route.back() == '/')
			route.pop_back();
		target = route + "/" + target;
	}

	return target;
}

Response::Response(Request const &req, Config const &conf) : _req(req), _conf(conf)
{
	static std::string	currentDir = std::filesystem::current_path();

	// If request has already been flagged as bad don't continue
	if (req.getStatus() == RequestStatus::Invalid
		|| req.getStatus() == RequestStatus::IdleTimeout
		|| req.getStatus() == RequestStatus::RecvTimeout) {
		_statusCode	= BadRequest;
		_body		= getErrorPageContent("400", _conf);

		formResponse();

		std::cout << "\n---- Response content ----\n" << _content << "\n";

		return;
	}

	// Be prepared for shenanigans, validate URI format for target early
	if (!uriFormatOk(req.getTarget()) || uriTargetAboveRoot(req.getTarget())) {
		_error		= ResponseError::BadTarget;
		_statusCode	= BadRequest;
		_body		= getErrorPageContent("400", _conf);

		formResponse();

		return;
	}

	// If the path has any ".." parts, remove them
	_target = std::filesystem::path(req.getTarget()).lexically_normal();

	_target = route(_target, _conf);

	// Handle directory listing and/or autoindexing
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
		_conf(other._conf),
		_headers(other._headers),
		_target(other._target),
		_startLine(other._startLine),
		_headerSection(other._headerSection),
		_body(other._body),
		_content(other._content),
		_statusCode(other._statusCode),
		_error(other._error)
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
			_startLine	= _req.getHttpVersion() + " 400 Bad Request";
			_body		= Pages::getPageContent("default400");
	}

	_headerSection += "Content-Length: " + std::to_string(_body.length()) + CRLF;

	_content = _startLine + CRLF + _headerSection + CRLF + _body;
}

void	Response::sendToClient()
{
	size_t	bytesToSend = _content.length() - _bytesSent;

	if (bytesToSend == 0)
		return;

	char const	*bufferPosition	= _content.c_str() + _bytesSent;
	ssize_t		bytesSent		= send(_req.getFd(), bufferPosition, bytesToSend, MSG_DONTWAIT);

	if (bytesSent < 0)
		throw std::runtime_error(ERROR_LOG("send: " + std::string(strerror(errno))));

	_bytesSent += bytesSent;
}

bool	Response::sendIsComplete()
{
	return _bytesSent >= _content.length();
}
