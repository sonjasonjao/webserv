#include <string>
#include <unordered_map>

class Request;

class Response {

	enum type {
		GET,
		POST,
	};

public:
	Response() = delete;
	Response(Request const &req);
	Response(Response const &other);
	~Response() = default;

	Response	&operator=(Response const &rhs);

	std::string const	&getContent();
	std::string const	&getField(std::string const &key);

private:

	using string_map = std::unordered_map<std::string, std::string>;

	Request const	&_req;
	std::string		_content;
	string_map		_fields;
};
