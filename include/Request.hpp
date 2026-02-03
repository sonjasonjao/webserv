#pragma once

#include "Response.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <memory>
#include <fstream>

#define IDLE_TIMEOUT			10000 // timeout values to be decided, and should they be in Server.hpp?
#define RECV_TIMEOUT			5000
#define SEND_TIMEOUT			5000
#define HEADERS_MAX_SIZE		8000
#define CLIENT_MAX_BODY_SIZE	1000000
#define CGI_TIMEOUT				4000

enum ResponseCode : int;

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
 * request, and thus client must be immediately disconnected without sending a response.
 */
enum class ClientStatus
{
	WaitingData,
	CgiRunning,
	CompleteReq,
	ReadyForResponse,
	IdleTimeout,
	RecvTimeout,
	SendTimeout,
	GatewayTimeout,
	Invalid,
	Error,
};

struct RequestLine
{
	std::string					target;
	std::optional<std::string>	query;
	std::string					httpVersion;
	RequestMethod				method;
	std::string					methodString;
};

/**
 * This structure stores both the metadata (headers) and the payload (data)
 * for a single part of a multipart message.
 *
 * - headers:      The raw HTTP header string for this part.
 * - name:         The form-field name (from Content-Disposition 'name').
 * - filename:     The client-side name of the file (from 'filename'), if provided.
 * - content_type: The MIME type of the data (e.g., "text/plain" or "image/png").
 * - data:         The raw content or binary body of the part.
 */

struct MultipartPart {
	std::string	 headers;
	std::string	 name;
	std::string	 filename;
	std::string	 contentType;
	std::string	 data;
};

/**
 * This structure stores all the data needed to process and form a response related to CGI
 * request handling.
 *
 * cgiPid			Child process ID which executes the CGI script
 * cgiStartTime		Script execution start time, tackle possible infinite loops
 * cgiResult		Output from the child process
 */
struct CgiRequest {
	using timePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

	pid_t		cgiPid = -1;
	timePoint	cgiStartTime;
	std::string	cgiResult;
};

class Request
{
	using stringMap = std::unordered_map<std::string, std::vector<std::string>>;
	using timePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

	private:
		int								_fd;
		int								_serverFd;
		ClientStatus					_status;
		ResponseCode					_responseCodeBypass;
		bool							_keepAlive;
		bool							_chunked;
		bool							_completeHeaders;
		std::unique_ptr<std::ofstream>	_uploadFD;
		timePoint						_idleStart;
		timePoint						_recvStart;
		timePoint						_sendStart;
		size_t							_headerSize;
		std::optional<size_t>			_contentLen;
		stringMap						_headers;
		struct RequestLine				_request;
		std::optional<CgiRequest>		_cgiRequest;
		std::string						_buffer;
		std::string						_body;
		std::optional<std::string>		_boundary;
		std::optional<std::string>		_uploadDir;

		void	parseRequest(void);
		void	parseRequestLine(std::string &req);
		void	parseHeaders(std::string &str);

		void	parseChunked();
		void	printData() const;

		bool	validateHeaders();
		bool	areValidChars(std::string &target);
		bool	validateAndAssignTarget(std::string &target);
		bool	validateAndAssignHttp(std::string &httpVersion);
		bool	isUniqueHeader(std::string const &key);
		bool	initialSaveToDisk(MultipartPart const &part);
		bool	saveToDisk(MultipartPart const &part);

	public:
		Request() = delete;
		Request(int fd, int serverFd);
		~Request() = default;

		void	processRequest(std::string const &buf);

		bool	isHeadersCompleted() const;
		bool	fillKeepAlive();
		bool	boundaryHasValue();

		void	reset();
		void	resetKeepAlive();

		void	checkReqTimeouts();
		void	setIdleStart();
		void	setRecvStart();
		void	setSendStart();
		void	setStatus(ClientStatus status);
		void	setKeepAlive(bool value);
		void	setResponseCodeBypass(ResponseCode code);

		void	resetSendStart();

		void	handleFileUpload();
		void	setUploadDir(std::string path);

		std::vector<std::string> const	*getHeader(std::string const &key) const;
		RequestMethod					getRequestMethod() const;
		ClientStatus					getStatus() const;
		ResponseCode					getResponseCodeBypass() const;
		std::string const				&getHttpVersion() const;
		std::string const				&getBody() const;
		std::string const				&getTarget() const;
		std::string const				&getBuffer() const;
		std::string const				&getMethodString() const;
		std::string						getHost() const;
		std::optional<std::string>		getQuery() const;
		size_t							getContentLength() const;
		bool							getKeepAlive() const;
		int								getFd() const;
		int								getServerFd() const;
		
		// class methods directly intercat with CGI handler
		bool							isCgiRequest() const;
		void							setCgiResult(std::string str);
		void        					setCgiPid(pid_t pid);
		void        					setCgiStartTime();
		void							printStatus() const;
		pid_t       					getCgiPid() const;
		timePoint   					getCgiStartTime() const;
		std::string						getCgiResult() const;
		stringMap						const &getHeaders(void) const;
};
