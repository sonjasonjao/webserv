#include "Response.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "Pages.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <sys/socket.h>
#include <unistd.h>

constexpr char const * const	CRLF = "\r\n";

static std::string			route(std::string target, Config const &conf);
static std::string const	&getResponsePageContent(std::string const &key, Config const &conf);
static std::string			getContentType(std::string sv);
static std::string			getDirectoryList(std::string_view target, std::string_view route);
static void					listify(std::vector<std::string> const &vec, size_t offset, std::stringstream &stream);

Response::Response(Request const &req, Config const &conf) : _req(req), _conf(conf)
{
	static std::string	startDir = std::filesystem::current_path();

	// If request has already been flagged as bad don't continue
	switch (req.getStatus()) {
		case RequestStatus::Invalid:
			_statusCode = BadRequest;
		break;
		case RequestStatus::RecvTimeout:
			_statusCode = RequestTimeout;
		break;
		default: break;
	}
	if (_statusCode != Unassigned) {
		formResponse();
		std::cout << "\n---- Response content ----\n";
		if (_contentType.find("image") == std::string::npos)
			std::cout << _content;
		else
			std::cout << "Image data...";
		std::cout << "\n--------------------------\n\n";

		return;
	}

	std::string	reqTarget = req.getTarget();

	// Be prepared for shenanigans, validate URI format for target early
	if (!uriFormatOk(reqTarget) || uriTargetAboveRoot(reqTarget)) {
		DEBUG_LOG("Bad target: " + reqTarget);
		_statusCode = BadRequest;

		formResponse();

		return;
	}

	// If the path has any ".." or "./" aka unnecessary parts, remove them
	_target = std::filesystem::path(reqTarget).lexically_normal();

	// Save original request target before routing
	reqTarget = _target;
	if (reqTarget != "/" && reqTarget.size() > 1 && reqTarget[0] == '/')
		reqTarget = reqTarget.substr(1);

	// Perform routing, aka mapping of a file or directory to another
	_target = route(_target, _conf);

	INFO_LOG("Routing " + reqTarget + " to " + _target);

	// Handle directory listing and/or autoindexing
	if (std::filesystem::is_directory(_target)) {
		DEBUG_LOG("Target '" + _target + "' is a directory" );
		if (!_conf.autoindex && !_conf.directoryListing) {
			DEBUG_LOG("Autoindexing and directory listing is disabled");
			_statusCode = BadRequest;

			formResponse();

			return;
		}

		if (_target.back() == '/')
			_target.pop_back();

		if (_conf.autoindex && !_conf.directoryListing) {
			_target = _target + "/" + "index.html";
		} else if (_conf.directoryListing) {
			_body = getDirectoryList(reqTarget, _target);
		}
	}

	std::string	searchDir = startDir;

	// If the route contains a '/' at the beginning define the search dir as root
	if (_target[0] == '/')
		searchDir = "/";

	// Check if resource can be found and set status code
	// NOTE: Match allowed methods from route
	switch (req.getRequestMethod()) {
		case RequestMethod::Get:
			if (!Pages::isCached(getAbsPath(_target)) && !resourceExists(_target, searchDir)) {
				INFO_LOG("Resource " + _target + " could not be found");
				_statusCode = NotFound;
				break;
			}
			if (Pages::isCached(getAbsPath(_target)))
				DEBUG_LOG("Resource " + _target + " found in cache");
			INFO_LOG("Resource " + _target + " found");
			_statusCode = OK;
		break;
		case RequestMethod::Post:
			INFO_LOG("Responding to POST request with target " + _target);
			_statusCode = OK;
		break;
		case RequestMethod::Delete:
			if (!resourceExists(_target, searchDir)) {
				INFO_LOG("Response: Resource " + _target + " could not be found");
				_statusCode = NotFound;
				break;
			}
			INFO_LOG("Resource " + _target + " deleted (not really but in the future)");
			_statusCode = NoContent;
		break;
		default:
			INFO_LOG("Unknown request method, response status defaulting to bad request");
			_statusCode = BadRequest;
		break;
	}

	formResponse();

	std::cout << "\n---- Response content ----\n";
	if (_contentType.find("image") == std::string::npos)
		std::cout << _content;
	else
		std::cout << "Image data...";
	std::cout << "\n--------------------------\n\n";
}

Response::Response(Response const &other)
	:	_req(other._req),
		_conf(other._conf),
		_target(other._target),
		_startLine(other._startLine),
		_headerSection(other._headerSection),
		_body(other._body),
		_content(other._content),
		_contentType(other._contentType),
		_statusCode(other._statusCode)
{}

std::string const	&Response::getContent() const
{
	return _content;
}

void	Response::formResponse()
{
	_headerSection =	std::string("Server: ") + _req.getHost() + CRLF;
	_headerSection +=	"Date: " + getImfFixdate() + CRLF;

	// If the body is non empty it means directory listing has been activated
	if (!_body.empty()) {
		_startLine		= _req.getHttpVersion() + " 200 OK";
		_contentType	= "text/html";
		_headerSection	+= "Content-Type: " + _contentType + std::string(CRLF);
		_headerSection	+= "Content-Length: " + std::to_string(_body.length()) + CRLF;
		_content		= _startLine + CRLF + _headerSection + CRLF + _body;

		return;
	}

	_contentType	=	getContentType(_target);
	_headerSection	+=	"Content-Type: " + _contentType + std::string(CRLF);

	switch (_statusCode) {
		case 200:
			_startLine	= _req.getHttpVersion() + " 200 OK";
			if (!_target.empty() && _target[0] == '/')
				_body	= Pages::getPageContent(_target);
			else if (!_target.empty())
				_body	= Pages::getPageContent(getAbsPath(_target));
			else
				_body	= getResponsePageContent("200", _conf);
		break;
		case 204:
			_startLine	= _req.getHttpVersion() + " 204 No Content";
			_body		= getResponsePageContent("204", _conf);
		break;
		case 400:
			_startLine	= _req.getHttpVersion() + " 400 Bad Request";
			_body		= getResponsePageContent("400", _conf);
		break;
		case 404:
			_startLine	= _req.getHttpVersion() + " 404 Not Found";
			_body		= getResponsePageContent("404", _conf);
		break;
		case 408:
			_startLine	= _req.getHttpVersion() + " 408 Request Timeout";
			_body		= getResponsePageContent("408", _conf);
		break;
		default:
			_startLine	= _req.getHttpVersion() + " 500 Internal Server Error";
			_body		= getResponsePageContent("500", _conf);
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

	DEBUG_LOG("Calling send to fd " + std::to_string(_req.getFd()));
	ssize_t		bytesSent		= send(_req.getFd(), bufferPosition, bytesToSend, MSG_DONTWAIT);

	if (bytesSent < 0)
		throw std::runtime_error(ERROR_LOG("send: " + std::string(strerror(errno))));

	_bytesSent += bytesSent;
}

bool	Response::sendIsComplete()
{
	return _bytesSent >= _content.length();
}

/* -------------------------------------------------------------------------- */

static std::string	route(std::string target, Config const &conf)
{
	// If the target isn't "/" remove any extra '/' characters, as they don't have any meaning (root access is forbidden)
	if (target.length() > 1 && target[0] == '/')
		target = target.substr(1);

	// Check for an exact route for target
	auto	it = conf.routes.find(target);

	if (it != conf.routes.end())
		return it->second;

	// Check for a partial route for target, exclude possible "/" substitution at this step
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

static std::string	getContentType(std::string target)
{
	std::string	contentType			= "text/html";
	auto		pos					= target.find_last_of(".");

	if (pos == std::string::npos)
		return contentType;

	auto		filenameExtension	= target.substr(pos);

	for (auto &c : filenameExtension)
		c = std::tolower(c);

	if (filenameExtension != ".html") {
		if (	 filenameExtension == ".jpeg"
			||	 filenameExtension == ".jpg")	contentType = "image/jpeg";
		else if	(filenameExtension == ".webp")	contentType = "image/webp";
		else if	(filenameExtension == ".png")	contentType = "image/png";
		else if	(filenameExtension == ".gif")	contentType = "image/gif";
		else if	(filenameExtension == ".css")	contentType = "text/css";
		else if	(filenameExtension == ".js")	contentType = "text/javascript";
		else if	(filenameExtension == ".json")	contentType = "application/json";
	}

	return contentType;
}

/**
 * NOTE: Assumes that the programmer is using correct error page number strings as keys
 */
static std::string const	&getResponsePageContent(std::string const &key, Config const &conf)
{
	// Check status pages if the key is a three digit number
	if (key.length() == 3 && std::all_of(key.begin(), key.end(), isdigit)) {
		auto	it = conf.status_pages.find(key);

		if (it != conf.status_pages.end() && resourceExists(it->second))
			return Pages::getPageContent(getAbsPath(it->second));
	} else {	// Othewise check normal routes
		auto	it = conf.routes.find(key);

		if (it != conf.routes.end() && resourceExists(it->second))
			return Pages::getPageContent(getAbsPath(it->second));
	}

	// Retrieve default
	return Pages::getPageContent("default" + key);
}

static std::string	getDirectoryList(std::string_view target, std::string_view route)
{
	std::vector<std::string>	files;
	std::vector<std::string>	directories;
	std::stringstream			stream;

	stream << "<!DOCTYPE html><html><head></head><body>\n";

	stream << "<h1>" << target << "</h1>";

	try {
		for (const auto &e : std::filesystem::directory_iterator(route)) {

			std::string	name = e.path().string();

			if (e.is_regular_file()) {
				auto	pos = std::upper_bound(files.begin(), files.end(), name);
				files.insert(pos, name);
			} else if (e.is_directory()) {
				auto	pos = std::upper_bound(directories.begin(), directories.end(), name);
				directories.insert(pos, name);
			}
		}
	} catch (std::exception const &e) {
		ERROR_LOG(std::string(strerror(errno)));
	}

	if (!directories.empty()) {
		stream << "<h2>Directories</h2>\n";
		listify(directories, route.length() + 1, stream);
	}

	if (!files.empty()) {
		stream << "\n<h2>Files</h2>\n";
		listify(files, route.length() + 1, stream);
	}

	stream << "</body></html>\n";

	return stream.str();
}

static void	listify(std::vector<std::string> const &vec, size_t offset, std::stringstream &stream)
{
	stream << "<ul>\n";

	for (const auto &v : vec) {
		stream << "<li>";
		stream << "<a href=\"" << v.substr(offset) << "\">";
		stream << v.substr(offset);
		stream << "</a>";
		stream << "</li>\n";
	}
	stream << "</ul>\n";
}
