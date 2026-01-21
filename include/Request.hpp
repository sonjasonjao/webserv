#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <memory>
#include "Utils.hpp"

#define IDLE_TIMEOUT			10000 // timeout values to be decided, and should they be in Server.hpp?
#define RECV_TIMEOUT			5000
#define SEND_TIMEOUT			5000
#define HEADERS_MAX_SIZE		8000
#define CLIENT_MAX_BODY_SIZE	1000000

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

/**
 * WaitingData is set for a new client by default, and ReadyForResponse whenever a response is
 * formed and ready to be sent from server. Error indicates there is a critical error in the HTTP
 * request, and thus client must be immediately disconnected.
 */
enum class RequestStatus
{
	WaitingData,
	CompleteReq,
	ReadyForResponse,
	IdleTimeout,
	RecvTimeout,
	SendTimeout,
	ContentTooLarge,
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
	using timePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

	private:

		int								_fd;
		int								_serverFd;
		RequestStatus					_status;
		bool							_keepAlive;
		bool							_chunked;
		bool							_completeHeaders;
		std::unique_ptr<std::ofstream> 	_uploadFD;
		timePoint						_idleStart;
		timePoint						_recvStart;
		timePoint						_sendStart;
		size_t							_headerSize;
		size_t							_curr_upload_pos;
		std::optional<size_t>			_contentLen;
		stringMap						_headers;
		struct RequestLine				_request;
		std::string						_buffer;
		std::string						_body;
		std::optional<std::string>		_boundary;
		std::string						_uploadDir;

	public:
		Request() = delete;
		Request(int fd, int serverFd);
		Request(Request&& ) = default;
		Request& operator=(Request&& ) = default;
		~Request() = default;

		void								saveRequest(std::string const &buf);
		void								handleRequest(void);
		void								parseRequest(void);
		void								parseRequestLine(std::string &req);
		void								parseHeaders(std::string &str);
		bool								fillKeepAlive(void);
		bool								validateHeaders(void);
		void								parseChunked(void);
		void								printData(void) const;
		bool								isUniqueHeader(std::string const &key);
		bool								validateAndAssignTarget(std::string &target);
		bool								areValidChars(std::string &target);
		bool								validateAndAssignHttp(std::string &httpVersion);
		void								setStatus(RequestStatus status);
		void								reset(void);
		void								resetKeepAlive(void);
		void								checkReqTimeouts(void);
		void								setIdleStart(void);
		void								setRecvStart(void);
		void								setSendStart(void);
		void								resetSendStart(void);
		void								resetBuffer(void);
		void								setUploadFD(std::unique_ptr<std::ofstream> outfile);
		std::ofstream*						getUploadFD(void);
		size_t								getCurrentUploadPosition(void);
		void								setCurrentUploadPosition(size_t pos);
		void								handleFileUpload(void);
		void								setUploadDir(std::string path);
		std::string							getUploadDir(void);

		RequestMethod						getRequestMethod(void) const;
		std::string const					&getHttpVersion(void) const;
		std::string const					&getBody(void) const;
		std::string const					&getTarget(void) const;
		std::vector<std::string> const		*getHeader(std::string const &key) const;
		std::string							getHost(void) const;
		int									getFd(void) const;
		int									getServerFd(void) const;
		bool								getKeepAlive(void) const;
		bool								isHeadersCompleted(void) const;
		size_t								getContentLength(void) const;
		RequestStatus						getStatus(void) const;
		std::string const					&getBuffer(void) const;
};
