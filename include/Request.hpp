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

enum class ReqStatus
{
	WaitingData,
	CompleteReq,
	ReadyForResponse,
	Invalid,
	Error
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
		int						_serverFd;
		std::string				_buffer;
		struct RequestLine		_request;
		stringMap				_headers;
		std::string				_body;
		std::optional<size_t>	_contentLen;
		bool					_keepAlive;
		bool					_chunked;
		bool					_completeHeaders;
		ReqStatus				_status;

	public:
		Request() = delete;
		Request(int fd, int serverFd);
		~Request() = default;

		void				saveRequest(std::string const& buf);
		void				handleRequest(void);
		void				parseRequest(void);
		void				fillHost(void);
		void				parseRequestLine(std::istringstream& req);
		void				parseHeaders(std::string& str);
		bool				validateHeaders(void);
		void				parseChunked(void);
		void				printData(void) const;
		bool				isUniqueHeader(std::string const& key);
		bool				isTargetValid(std::string& target);
		bool				areValidChars(std::string& target);
		bool				isHttpValid(std::string& httpVersion);
		std::string			getHost(void) const;
		int					getFd(void) const;
		int					getServerFd(void) const;
		bool				getKeepAlive(void) const;
		ReqStatus			getStatus(void) const;
		void				setStatus(ReqStatus status);
		std::string const	&getBuffer(void) const;
		void				reset(void);
		void				resetKeepAlive(void);

		RequestMethod						getRequestMethod(void) const;
		std::string const					&getHttpVersion(void) const;
		std::string const					&getBody(void) const;
		std::string const					&getTarget(void) const;
		std::vector<std::string> const *	getHeader(std::string const &key) const;
};
