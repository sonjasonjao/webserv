#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

std::string	getImfFixdate();

constexpr char const * const	CRLF = "\r\n";

enum	methodType : int {
	GET,
	POST,
	DELETE,
};

class Request {

public:
	methodType	getType() const;
private:
	methodType	_type;
};

class Response {

public:
	Response() = delete;
	Response(Request const &req);
	Response(Response const &other);
	~Response() = default;

	std::string const								&getContent();
	std::optional<std::vector<std::string> const *>	getHeader(std::string const &key);
	methodType										getType();

private:

	using strVecMap	= std::unordered_map<std::string, std::vector<std::string> >;

	Request const	&_req;
	strVecMap		_headers;
	std::string		_startLine;
	std::string		_headerSection;
	std::string		_body;
	std::string		_content;
};

Response::Response(Request const &req) : _req(req)
{
	// Is the request already marked as bad?
	//	form request that is correct for the error type

	// Validate request
	//	date, fields, lenghts

	// Match request type
	//	if ok
	//		get correct config
	//		get correct content
	//			from file or from memory?
	//			What if the content is huge? Chunking time?

	_body = "<DOCTYPE! html><html><head></head><body>Hi</body></html>";
	switch (req.getType()) {
		case GET:
			_startLine = "HTTP/1.1 200 OK";
			break;
		case POST:
			_startLine = "HTTP/1.1 200 OK";
			break;
		case DELETE:
			_startLine = "HTTP/1.1 200 OK";
			break;
	}
	_headerSection = std::string("Server: webserv") + CRLF;
	_headerSection += "Date: " + getImfFixdate() + CRLF;
	_headerSection += "Content-Length: " + std::to_string(_body.size()) + CRLF;
	_headerSection += std::string("Content-Type: text/html") + CRLF;

	_content = _startLine + _headerSection + CRLF + _body;
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

methodType	Response::getType()
{
	return _req.getType();
}
