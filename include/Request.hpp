#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>

/**
 * Mandatory methods required in the subject, do we want to add more? -> Will affect
 * request parsing and possibly class member attributes.
 */
enum class RequestMethod
{
	Get,
	Post,
	Delete,
	Unknown
};

struct RequestLine
{
	std::string					target;
	std::optional<std::string>	query;
	std::string					httpVersion;
	RequestMethod				method;
};

class Request
{
	using stringMap = std::unordered_map<std::string, std::vector<std::string>>;

	private:
		int						_fd;
		std::string				_buffer;
		struct RequestLine		_request;
		stringMap				_headers;
		std::string				_body;
		std::optional<size_t>	_contentLen;
		bool					_keepAlive;
		bool					_chunked;
		bool					_isValid;
		bool					_isMissingData;

	public:
		Request() = delete;
		Request(int fd);
		~Request() = default;

		void			saveRequest(std::string const& buf);
		void			handleRequest(void);
		void			parseRequest(void);
		void			parseRequestLine(std::istringstream& req);
		void			parseHeaders(std::string& str);
		bool			validateHeaders(void);
		void			parseChunked(void);
		void			printData(void) const;
		bool			isUniqueHeader(std::string const& key);
		bool			isTargetValid(std::string& target);
		bool			areValidChars(std::string& target);
		bool			isHttpValid(std::string& httpVersion);
		std::string		getHost(void);
		int				getFd(void) const;
		bool			getKeepAlive(void) const;
		bool			getIsValid(void) const;
		bool			getIsMissingData(void) const;
		bool			isBufferEmpty(void);
		void			reset(void);

		RequestMethod		getRequestMethod(void) const;
		std::string const	&getHttpVersion(void) const;
		std::string const	&getBody(void) const;
		std::string const	&getTarget(void) const;
		std::optional<std::vector<std::string> const *>	getHeader(std::string const &key) const;
};
