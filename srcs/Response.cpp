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

	std::string const			&getContent();
	std::optional<std::string>	getField(std::string const &key);
	methodType					getType();

private:

	using strMap	= std::unordered_map<std::string, std::string>;
	using strVecMap	= std::unordered_map<std::string, std::vector<std::string> >;

	Request const	&_req;
	strMap			_fields;
	std::string		_startLine;
	std::string		_headers;
	std::string		_body;
	std::string		_content;
};

Response::Response(Request const &req) : _req(req)
{
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
	_headers = std::string("Server: webserv") + CRLF;
	_headers += "Date: " + getImfFixdate() + CRLF;
	_headers += "Content-Length: " + std::to_string(_body.size()) + CRLF;
	_headers += std::string("Content-Type: text/html") + CRLF;

	_content = _startLine + _headers + CRLF + _body;
}

Response::Response(Response const &other) : _req(other._req) {}
