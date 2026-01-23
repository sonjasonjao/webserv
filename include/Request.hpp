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

enum ResponseCode : int;

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
	Invalid,
	Error,
};

struct RequestLine
{
	std::string					target;
	std::optional<std::string>	query;
	std::string					httpVersion;
	RequestMethod				method;
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
    std::string headers;
    std::string name;
    std::string filename;
    std::string contentType;
    std::string data;
};

class Request
{
	using stringMap = std::unordered_map<std::string, std::vector<std::string>>;
	using timePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

	private:
		int								_fd;
		int								_serverFd;
		RequestStatus					_status;
		ResponseCode					_responseCodeBypass;
		bool							_keepAlive;
		bool							_chunked;
		bool							_completeHeaders;
		std::unique_ptr<std::ofstream>	_uploadFD;
		timePoint						_idleStart;
		timePoint						_recvStart;
		timePoint						_sendStart;
		size_t							_headerSize;
		size_t							_currUploadPos;
		std::optional<size_t>			_contentLen;
		stringMap						_headers;
		struct RequestLine				_request;
		std::string						_buffer;
		std::string						_body;
		std::optional<std::string>		_boundary;
		std::optional<std::string>		_uploadDir;

		void	initialSaveToDisk(const MultipartPart& part);
		void	saveToDisk(const MultipartPart& part);

	public:
		Request() = delete;
		Request(int fd, int serverFd);
		~Request() = default;

		void	saveRequest(std::string const &buf);
		void	handleRequest(void);
		void	parseRequest(void);
		void	parseRequestLine(std::string &req);
		void	parseHeaders(std::string &str);

		void	parseChunked(void);
		void	printData(void) const;

		bool	validateHeaders(void);
		bool	areValidChars(std::string &target);
		bool	validateAndAssignTarget(std::string &target);
		bool	validateAndAssignHttp(std::string &httpVersion);
		bool	isUniqueHeader(std::string const &key);
		bool	isHeadersCompleted(void) const;
		bool	fillKeepAlive(void);

		void	reset(void);
		void	resetKeepAlive(void);

		void	checkReqTimeouts(void);
		void	setIdleStart(void);
		void	setRecvStart(void);
		void	setSendStart(void);
		void	setStatus(RequestStatus status);
		void	setResponseCodeBypass(ResponseCode code);

		void	resetSendStart(void);
		void	resetBuffer(void);

		void	handleFileUpload(void);
		void	setUploadDir(std::string path);

		std::vector<std::string> const	*getHeader(std::string const &key) const;
		RequestMethod					getRequestMethod(void) const;
		RequestStatus					getStatus(void) const;
		ResponseCode					getResponseCodeBypass(void) const;
		std::string const				&getHttpVersion(void) const;
		std::string const				&getBody(void) const;
		std::string const				&getTarget(void) const;
		std::string const				&getBuffer(void) const;
		std::string						getHost(void) const;
		size_t							getContentLength(void) const;
		bool							getKeepAlive(void) const;
		int								getFd(void) const;
		int								getServerFd(void) const;
};
