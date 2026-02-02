#include "Response.hpp"
#include "Request.hpp"
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

static Route				getRoute(std::string uri, Config const &conf);
static std::string const	&getResponsePageContent(std::string const &key, Config const &conf);
static std::string			getContentType(std::string sv);
static void					listify(std::vector<std::string> const &vec, size_t offset, std::stringstream &stream);

/**
 * Main functionality for response forming. Categorizes links information from the source request
 * and matching configuration, validates relevant fields, picks the correct response status code,
 * and constructs the matching response content buffer.
 */
Response::Response(Request const &req, Config const &conf) : _req(req), _conf(conf)
{
	INFO_LOG("Forming response for " + _req.getMethodString() + " request targeting " + _req.getTarget());
	/* Already flagged requests */

	ResponseCode	bypass = _req.getResponseCodeBypass();

	if (bypass != Unassigned) {
		_statusCode = bypass;
		formResponse();

		return;
	}

	switch (_req.getStatus()) {
		case ClientStatus::Invalid:			_statusCode = BadRequest;		break;
		case ClientStatus::RecvTimeout:		_statusCode = RequestTimeout;	break;
		default: break;
	}
	if (_statusCode != Unassigned) {
		formResponse();

		debugPrintResponseContent();

		return;
	}

	/* Target URI validation */

	_target = _req.getTarget();

	if (!uriFormatOk(_target) || uriTargetAboveRoot(_target)) {
		DEBUG_LOG("Bad target: " + _req.getTarget());
		_statusCode = BadRequest;
		formResponse();

		return;
	}

	/* Target URI sanitation */

	// If the path has any ".." or "./" aka unnecessary parts, remove them
	_target = std::filesystem::path(_req.getTarget()).lexically_normal();

	// Save original sanitized request target before routing
	_reqTargetSanitized = _target;
	if (_reqTargetSanitized.size() > 1 && _reqTargetSanitized[0] == '/')
		_reqTargetSanitized = _reqTargetSanitized.substr(1);

	routing();	// Routing possibly remaps the target path

	/* Forbidden methods */

	auto const	*okMethods = &_route.allowedMethods;

	// If there  is no route level restrictions, look for server level restrictions
	if (!okMethods->has_value())
		okMethods = &_conf.allowedMethods;

	if (okMethods->has_value()) {
		std::string	methodString = _req.getMethodString();

		auto const	&m = *okMethods;

		if (methodString.empty() || std::find(m->begin(), m->end(), methodString) == m->end()) {
			DEBUG_LOG("Method '" + methodString + "' not in list of allowed methods");
			_statusCode = Forbidden;
			formResponse();

			return;
		}
	}

	if (req.getRequestMethod() == RequestMethod::Delete) {
		handleDelete();
		formResponse();
		debugPrintResponseContent();
		return;
	}
	else if (!_route.original.empty())
		INFO_LOG("Routing " + _reqTargetSanitized + " to " + _target);

	/* Directory targets */

	if (_req.getRequestMethod() == RequestMethod::Get && std::filesystem::is_directory(_target))
		handleDirectoryTarget();

	if (_statusCode == Forbidden && _req.getRequestMethod() == RequestMethod::Get) {
		formResponse();

		return;
	}

	locateTargetAndSetStatusCode();

	formResponse(); /* ---------- Forming the contents of the response buffer */

	debugPrintResponseContent();
}

/* --------------------------------------------------------- Public functions */

/**
 * @return	Response content buffer string
 */
std::string const	&Response::getContent() const
{
	return _content;
}

/**
 * @return	Response status code
 */
int	Response::getStatusCode() const
{
	return _statusCode;
}

/**
 * Function to send response to client, keeps track of already sent bytes and
 * can be called repeatedly.
 */
void	Response::sendToClient()
{
	size_t const	bytesToSend		= _content.length() - _bytesSent;
	char const		*bufferPosition	= _content.c_str() + _bytesSent;

	if (bytesToSend == 0) {
		DEBUG_LOG("Complete response already sent");
		return;
	}

	DEBUG_LOG("Calling send to fd " + std::to_string(_req.getFd()));

	ssize_t const	bytesSent = send(_req.getFd(), bufferPosition, bytesToSend, MSG_DONTWAIT);

	if (bytesSent < 0) {
		ERROR_LOG("send: " + std::string(strerror(errno)));
		return;
	}

	_bytesSent += bytesSent;
}

/**
 * @return	true if all bytes in the response content buffer have been sent, false if not
 */
bool	Response::sendIsComplete() const
{
	return _bytesSent >= _content.length();
}

/* -------------------------------------------------------- Private functions */

/**
 * Uses previously gathered context to form the response buffer string.
 */
void	Response::formResponse()
{
	_headerSection  = std::string("Server: ") + _conf.serverName + CRLF;
	_headerSection += "Date: " + getImfFixdate() + CRLF;

	if (_directoryListing) {
		_body = getDirectoryList(_reqTargetSanitized, _target);

		if (_statusCode != InternalServerError) {
			_startLine		 = _req.getHttpVersion() + " 200 OK";
			_contentType	 = "text/html";
			_headerSection	+= "Content-Type: " + _contentType + std::string(CRLF);
			_headerSection	+= "Content-Length: " + std::to_string(_body.length()) + CRLF;
			_content		 = _startLine + CRLF + _headerSection + CRLF + _body;

			return;
		}
	}

	if (_statusCode == 200)
		_contentType = getContentType(_target);
	else
		_contentType = "text/html";

	_headerSection += "Content-Type: " + _contentType + std::string(CRLF);

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
		case 207:
			_startLine	= _req.getHttpVersion() + " 207 Created";
			_body		= getResponsePageContent("207", _conf);
		break;
		case 400:
			_startLine	= _req.getHttpVersion() + " 400 Bad Request";
			_body		= getResponsePageContent("400", _conf);
		break;
		case 403:
			_startLine	= _req.getHttpVersion() + " 403 Forbidden";
			_body		= getResponsePageContent("403", _conf);
		break;
		case 404:
			_startLine	= _req.getHttpVersion() + " 404 Not Found";
			_body		= getResponsePageContent("404", _conf);
		break;
		case 408:
			_startLine	= _req.getHttpVersion() + " 408 Request Timeout";
			_body		= getResponsePageContent("408", _conf);
		break;
		case 409:
			_startLine	= _req.getHttpVersion() + " 409 Conflict";
			_body		= getResponsePageContent("409", _conf);
		break;
		case 413:
			_startLine	= _req.getHttpVersion() + " 413 Content Too Large";
			_body		= getResponsePageContent("413", _conf);
		break;
		case 422:
			_startLine	= _req.getHttpVersion() + " 422 Unprocessable content";
			_body		= getResponsePageContent("422", _conf);
		break;
		default:
			_startLine	= _req.getHttpVersion() + " 500 Internal Server Error";
			_body		= getResponsePageContent("500", _conf);

			if (!_diagnosticMessage.empty()) {
				auto	pos = _body.find("</body>");

				if (pos != std::string::npos)
					_body.insert(pos, "<p>" + _diagnosticMessage + "</p>");
			}
		break;
	}

	_headerSection += "Content-Length: " + std::to_string(_body.length()) + CRLF;

	_content = _startLine + CRLF + _headerSection + CRLF + _body;
}

void	Response::routing()
{
	// Look for a route, which is a redirection of a URI/part of a URI to a specific folder or file on the server
	_route = getRoute(_target, _conf);

	// A non empty string signifies a matched route
	if (!_route.original.empty()) {
		if (_target.length() > 1 && _target[0] == '/')
			_target = _target.substr(1);
		if (_target.back() == '/')
			_target.pop_back();
		if (_route.original == "/") {
			if (_route.target.back() == '/')
				_route.target.pop_back();
			_target = _route.target + "/" + _target;
		} else {
			_target.replace(_target.find(_route.original), _route.original.length(), _route.target);
		}
	}
}

void	Response::handleDelete()
{
	_target = _reqTargetSanitized;

	// If uploadDir is not given in config, file deletion is disabled
	if (!_conf.uploadDir.has_value()) {
		_statusCode = Forbidden;
		return;
	}

	std::string	uploadDir = _conf.uploadDir.value();

	// Target will be modified if needed for correct format: '/' is removed
	// from start, but is needed between uploadDir and target given in request
	if (_target.length() > 1 && _target[0] == '/')
		_target = _target.substr(1);
	if (uploadDir.back() == '/')
		uploadDir.pop_back();
	_target = uploadDir + "/" + _target;

	if (!resourceExists(_target)) {
		INFO_LOG("Resource '" + _target + "' could not be found");
		_statusCode = NotFound; // do we disconnect client?

		return;
	}

	if (std::filesystem::is_directory(_target)) {
		INFO_LOG("Resource '" + _target + "' is a directory");
		_statusCode = Forbidden;

		return;
	}

	try {
		bool	ret = std::filesystem::remove(_target);

		if (!ret)
			throw std::runtime_error("");

		INFO_LOG("Resource " + _target + " deleted");
		_statusCode = NoContent;
	} catch (std::exception &e) {
		INFO_LOG("Resource " + _target + " could not be deleted");
		_statusCode = InternalServerError; // do we disconnect client?
		_diagnosticMessage = "Target '" + _target + "' could not be deleted";
	}
}

void	Response::handleDirectoryTarget()
{
	DEBUG_LOG("Target '" + _target + "' is a directory");
	if (!_conf.autoindex && !_conf.directoryListing) {
		DEBUG_LOG("Autoindexing and directory listing is disabled");
		_statusCode = Forbidden;

		return;
	}

	if (_target.back() == '/')
		_target.pop_back();

	// Directory listing is prioritized over autoindexing
	if (_conf.directoryListing) {
		_directoryListing	= true;
	} else if (_conf.autoindex) {
		_target = _target + "/" + "index.html";
	}
}

/**
 * @return	String containing the HTML page with links to the contents of the directory
 */
std::string	Response::getDirectoryList(std::string_view target, std::string_view route)
{
	std::vector<std::string>	files;
	std::vector<std::string>	directories;
	std::stringstream			stream;

	stream << "<!DOCTYPE html><html><head></head><body>\n";

	stream << "<h1>" << target << "</h1>";

	try {
		for (auto const &e : std::filesystem::directory_iterator(route)) {

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
		std::stringstream	errorMsg;

		errorMsg << e.what();
		if (errno != 0)
			errorMsg << ", errno = " << std::to_string(errno) << ": " << std::string(strerror(errno));
		ERROR_LOG(errorMsg.str());
		_diagnosticMessage = "Error creating directory list";
		_statusCode = InternalServerError;

		return "";
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

void	Response::locateTargetAndSetStatusCode()
{
	static std::string const	startDir	= std::filesystem::current_path();
	std::string					searchDir	= startDir;

	// If the route contains a '/' at the beginning, define the search dir as root
	if (_target[0] == '/')
		searchDir = "/";

	// Check if resource can be found and set status code
	switch (_req.getRequestMethod()) {
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
		default:
			INFO_LOG("Unknown request method, response status defaulting to bad request");
			_statusCode = BadRequest;
		break;
	}
}

void	Response::debugPrintResponseContent()
{
	#if DEBUG_LOGGING
	std::cout << "\n---- Response content ----\n";
	if (_contentType.find("image") == std::string::npos)
		std::cout << _content;
	else
		std::cout << "Image data...";
	std::cout << "\n--------------------------\n\n";
	#endif
}

/* --------------------------------------------------------- Static functions */

/**
 * @return	Route struct containing the information of the matched route,
 *			empty struct if no route can be matched
 */
static Route	getRoute(std::string uri, Config const &conf)
{
	// If the target isn't "/" remove any extra '/' characters, as they don't have any meaning (root access is forbidden)
	if (uri.length() > 1 && uri[0] == '/')
		uri = uri.substr(1);

	// Check for an exact route for target
	auto	it = conf.routes.find(uri);

	if (it != conf.routes.end())
		return it->second;

	// Check for a partial route for target, exclude possible "/" substitution at this step
	for (auto const &[key, val] : conf.routes) {
		if (key == "/")
			continue;

		auto	pos = uri.find(key);

		if (pos != std::string::npos)
			return val;
	}

	// If a route for "/" exists use that in case of no exact or partial match
	it = conf.routes.find("/");
	if (it != conf.routes.end())
		return it->second;

	return Route();
}

/**
 * @return	string matching the content type of the target, determined by file extension
 */
static std::string	getContentType(std::string target)
{
	std::string	contentType	= "text/html";
	auto		pos			= target.find_last_of(".");

	if (pos == std::string::npos)
		return contentType;

	auto	filenameExtension	= target.substr(pos);

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
 *
 * @return	Constant string reference to matching page
 */
static std::string const	&getResponsePageContent(std::string const &key, Config const &conf)
{
	// Check status pages if the key is a three digit number
	if (key.length() == 3 && std::all_of(key.begin(), key.end(), isdigit)) {
		auto	it = conf.statusPages.find(key);

		if (it != conf.statusPages.end() && resourceExists(it->second))
			return Pages::getPageContent(getAbsPath(it->second));
	} else {	// Othewise check normal routes
		auto	it = conf.routes.find(key);

		if (it != conf.routes.end() && resourceExists(it->second.target))
			return Pages::getPageContent(getAbsPath(it->second.target));
	}

	// Retrieve default
	return Pages::getPageContent("default" + key);
}

/**
 * Helper function for forming directory lists, creates HTML unordered list out of a vector.
 */
static void	listify(std::vector<std::string> const &vec, size_t offset, std::stringstream &stream)
{
	stream << "<ul>\n";

	for (auto const &v : vec) {
		stream << "<li>";
		stream << "<a href=\"" << v.substr(offset) << "\">";
		stream << v.substr(offset);
		stream << "</a>";
		stream << "</li>\n";
	}
	stream << "</ul>\n";
}
