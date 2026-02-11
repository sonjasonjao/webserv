#include "Server.hpp"
#include "Log.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CgiHandler.hpp"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <filesystem>

volatile sig_atomic_t	endSignal = false;

using ReqIter = std::list<Request>::iterator;

/**
 * Handles SIGINT signal by updating the value of global endSignal variable (to stop poll() loop
 * and eventually close the server).
 */
void	handleSignal(int sig)
{
	endSignal = sig;
}

/**
 * At construction, server starts listening to SIGINT, _configs will be fetched from parser,
 * and grouped for correct server socket creation.
 */
Server::Server(Parser &parser)
{
	signal(SIGINT, handleSignal);
	_configs = parser.getServerConfigs();
	groupConfigs();
}

/**
 * Loops through existing serverGroups and checks whether any of them shares the same
 * IP and port with this current config.
 */
bool	Server::isGroupMember(Config &conf)
{
	for (auto it = _serverGroups.begin(); it != _serverGroups.end(); it++) {
		if (it->defaultConf->host == conf.host && it->defaultConf->port == conf.port) {
			it->configs.emplace_back(conf);

			return true;
		}
	}
	return false;
}

/**
 * Groups configs so that all configs in one group have the same IP and the same port.
 * Each serverGroup will then have one server (listener) socket. The first config added in a
 * server group will be the default config of the group.
 */
void	Server::groupConfigs()
{
	for (auto it = _configs.begin(); it != _configs.end(); it++) {
		if (_serverGroups.empty() || !isGroupMember(*it)) {
			ServerGroup	newServGroup;
			newServGroup.fd = -1;
			newServGroup.configs.emplace_back(*it);
			newServGroup.defaultConf = &(*it);
			_serverGroups.emplace_back(newServGroup);
		}
	}
}

/**
 * Creates a server socket, binds it to correct address, and starts listening.
 */
int	Server::createSingleServerSocket(Config conf)
{
	int	listener;
	int	yes = 1;
	int	ret;
	struct addrinfo	hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(conf.host.c_str(), (std::to_string(conf.port)).c_str(),
		&hints, &servinfo);
	if (ret != 0)
		throw std::runtime_error(ERROR_LOG("getaddrinfo: " + std::string(gai_strerror(ret))));

	for (p = servinfo; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			ERROR_LOG("socket: " + std::string(strerror(errno)));
			continue;
		}
		if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
			ERROR_LOG("setsockopt: " + std::string(strerror(errno)));
			close(listener);
			continue;
		}
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			ERROR_LOG("bind: " + std::string(strerror(errno)));
			close(listener);
			continue;
		}
		if (fcntl(listener, F_SETFL, O_NONBLOCK) < 0) {
			ERROR_LOG("fcntl: " + std::string(strerror(errno)));
			close(listener);
			continue;
		}
		break;
	}
	if (p == NULL) {
		freeaddrinfo(servinfo);
		throw std::runtime_error(ERROR_LOG("Could not create server socket(s)"));
	}

	freeaddrinfo(servinfo);

	if (listen(listener, MAX_PENDING) < 0) {
		close(listener);
		throw std::runtime_error(ERROR_LOG("listen: " + std::string(strerror(errno))));
	}

	INFO_LOG("Server listening on fd " + std::to_string(listener));

	return listener;
}

/**
 * Loops through serverGroups and creates listener socket for each, stores
 * them into _pfds and stores the fd of the created socket into that serverGroup.
 */
void	Server::createServerSockets()
{
	for (auto it = _serverGroups.begin(); it != _serverGroups.end(); it++) {
		int sockfd = createSingleServerSocket(*(it->defaultConf));
		_pfds.push_back({ sockfd, POLLIN, 0 });
		it->fd = sockfd;
	}
}

/**
 * Calls getServerSockets() to create listener sockets, starts poll() loop. If a signal is
 * detected, it gets caught with poll returning -1 with errno set to EINTR --> continues
 * to next loop round, on which endSignal won't be false, and loop will finish.
 */
void	Server::run()
{
	createServerSockets();

	while (endSignal == false) {
		int	pollCount = poll(_pfds.data(), _pfds.size(), POLL_TIMEOUT);
		if (pollCount < 0) {
			if (errno == EINTR)
				continue;
			throw std::runtime_error(ERROR_LOG("poll: " + std::string(strerror(errno))));
		}
		handleConnections();
	}

	if (endSignal == SIGINT) {
		std::cout << '\n';
		INFO_LOG("Server closed with SIGINT signal");
	}
}

/**
 * Accepts new client connection, stores the fd into _pfds, and creates a Request object for the
 * client in _clients.
 */
void	Server::handleNewClient(int listener)
{
	struct sockaddr_storage	newClient;
	socklen_t				addrLen = sizeof(newClient);
	int						clientFd;

	INFO_LOG("Handling new client connecting on fd " + std::to_string(listener));

	clientFd = accept(listener, (struct sockaddr*)&newClient, &addrLen);

	if (clientFd < 0)
		throw std::runtime_error(ERROR_LOG("accept: " + std::string(strerror(errno))));

	if (_clients.size() >= MAX_CLIENTS) {
		DEBUG_LOG("Connected clients limit reached, unable to accept new client");
		close(clientFd);

		return;
	}

	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0) {
		close(clientFd);
		throw std::runtime_error(ERROR_LOG("fcntl: " + std::string(strerror(errno))));
	}

	_pfds.push_back({ clientFd, POLLIN, 0 });
	_clients.emplace_back(clientFd, listener);
	INFO_LOG("New client accepted, assigned fd " + std::to_string(clientFd));
}

/**
 * Finds the Request object of the client that poll() has recognized to have sent something,
 * calls recv() to get the data, and parses the request.
 */
void	Server::handleClientData(size_t &i)
{
	if (_clients.empty())
		throw std::runtime_error(ERROR_LOG("Could not find request with fd "
			+ std::to_string(_pfds[i].fd) + ", clients list empty"));

	auto	it = getRequestByFd(_pfds[i].fd);

	if (it == _clients.end())
		throw std::runtime_error(ERROR_LOG("Could not find request with fd "
			+ std::to_string(_pfds[i].fd)));

	if (it->getStatus() != ClientStatus::WaitingData
		&& it->getStatus() != ClientStatus::CgiRunning)
		return;

	INFO_LOG("Handling client data from fd " + std::to_string(_pfds[i].fd));

	char	buf[RECV_BUF_SIZE + 1];

	ssize_t	numBytes = recv(_pfds[i].fd, buf, RECV_BUF_SIZE, 0);

	if (numBytes <= 0) {
		if (numBytes == 0)
			INFO_LOG("Client disconnected on fd " + std::to_string(_pfds[i].fd));
		else
			ERROR_LOG("recv: " + std::string(strerror(errno)));

		removeClientFromPollFds(i);

		INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
		cleanupCgi(&(*it));
		_clients.erase(it);
		return;
	}
	buf[numBytes] = '\0';

	INFO_LOG("Received client data from fd " + std::to_string(_pfds[i].fd));

	#if DEBUG_LOGGING
	std::cout << "\n---- Request data ----\n" << buf << "----------------------\n\n";
	#endif

	it->setIdleStart();
	it->setRecvStart();
	it->processRequest(std::string(buf, numBytes));
    processClientRequest(i, it);
}

/**
 * Builds the response to be sent to client, resets Request properties, and sets client status
 * to ReadyForResponse.
 */
void	Server::prepareResponse(Request &req, Config const &conf)
{
	INFO_LOG("Building response to client fd " + std::to_string(req.getFd()));
	_responses[req.getFd()].emplace_back(Response(req, conf));
	req.reset();
	req.setStatus(ClientStatus::ReadyForResponse);
}

/**
 * Matches current request (so, client) with the config of the server it is connected to.
 * Looks for the serverGroup with a matching server fd, and then looks for the host name to
 * match Host header value in the request. If no host name match is found, returns the
 * default config of that serverGroup.
 */
Config const	&Server::matchConfig(Request const &req)
{
	int			fd				= req.getServerFd();
	ServerGroup	*serverGroup	= nullptr;

	for (auto it = _serverGroups.begin(); it != _serverGroups.end(); it++) {
		if (it->fd == fd) {
			serverGroup = &(*it);
			break;
		}
	}
	if (serverGroup == nullptr)
		throw std::runtime_error(ERROR_LOG("Unexpected error in matching request with server config"));
	for (auto it = serverGroup->configs.begin(); it != serverGroup->configs.end(); it++) {
		if (it->serverName == req.getHost()) {
			DEBUG_LOG("Matched configuration: " + it->serverName);
			return *it;
		}
	}
	DEBUG_LOG("No matching config, using default: " + serverGroup->defaultConf->serverName);
	return *(serverGroup->defaultConf);
}

/**
 * In case of a client that has disconnected itself, or will be disconnected (invalid request,
 * critical error in request, timeout, or keepAlive being false), this function closes its fd
 * and removes it from _pfds.
 */
void	Server::removeClientFromPollFds(size_t &i)
{
	INFO_LOG("Closing fd " + std::to_string(_pfds[i].fd));
	close(_pfds[i].fd);

	if (_pfds.size() > (i + 1)) {
		DEBUG_LOG("Overwriting fd " + std::to_string(_pfds[i].fd) + " with fd "
			+ std::to_string(_pfds[_pfds.size() - 1].fd));
		INFO_LOG("Removing client fd " + std::to_string(_pfds[i].fd) + " from poll list");
		_pfds[i] = _pfds[_pfds.size() - 1];
		_pfds.pop_back();
		i--;

		return;
	}

	INFO_LOG("Removing client fd " + std::to_string(_pfds.back().fd) + ", last client");
	_pfds.pop_back();
	i--;
}

/**
 * Sets the starting time for send timeout tracking and calls sendToClient().
 * If the response was completely sent with one call, removes sent response from _responses,
 * resets the send timeout tracker to 0, and removes POLLOUT from events. In case of keepAlive
 * being false, disconnects and removes the client; in case of keepAlive, sets client status
 * back to WaitingData.
 */
void	Server::sendResponse(size_t &i)
{
	auto	it = getRequestByFd(_pfds[i].fd);
	if (it == _clients.end())
		throw std::runtime_error(ERROR_LOG("Could not find request with fd "
			+ std::to_string(_pfds[i].fd)));
	if (it->getStatus() != ClientStatus::ReadyForResponse
		&& it->getStatus() != ClientStatus::RecvTimeout
		&& it->getStatus() != ClientStatus::GatewayTimeout)
		return;

	try {
		auto	&res = _responses.at(_pfds[i].fd).front();

		INFO_LOG("Sending response to client fd " + std::to_string(_pfds[i].fd));
		res.sendToClient();
		if (!res.sendIsComplete()) {
			INFO_LOG("Response partially sent, waiting for server to complete response sending");
			return;
		}

		if ((res.getStatusCode() / 100) != 2)
			it->setKeepAlive(false);
	} catch (std::exception const &e) {
		throw std::runtime_error(ERROR_LOG("Unexpected error in finding response for fd "
			+ std::to_string(_pfds[i].fd)));
	}

	DEBUG_LOG("Removing front element of _responses container for fd " + std::to_string(_pfds[i].fd));
	_responses.at(_pfds[i].fd).pop_front();

	it->resetSendStart();
	_pfds[i].events &= ~POLLOUT;

	DEBUG_LOG("Keep alive status: " + std::to_string(it->getKeepAlive()));
	if (it->getStatus() == ClientStatus::Invalid || !it->getKeepAlive()) {
		removeClientFromPollFds(i);
		INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
		cleanupCgi(&(*it));
		_clients.erase(it);

		return;
	}

	it->resetKeepAlive();
	it->setStatus(ClientStatus::WaitingData);
    // Once the response send re-enable clientFD for reading
    _pfds[i].events |= POLLIN;
    // Since there can be requests already read and stored in the buffer, immidiately
    // start to process the left-over in the bufer
    if (!it->getBuffer().empty())
    {
        it->processRequest("");
        processClientRequest(i, it);
    }
}

/**
 * Helper to get the Request object that matches fd given as parameter.
 */
ReqIter	Server::getRequestByFd(int fd)
{
	for (auto it = _clients.begin(); it != _clients.end(); it++) {
		if (it->getFd() == fd)
			return it;
	}
	return _clients.end();
}

/**
 * On each poll round, checks whether any of the clients have experienced idle, receive, or
 * send timeout. If an receive timeout occurs, calls Response constructor to
 * form an error page response, and sendResponse to send it and to disconnect client. In
 * case of idle or send timeout, client is disconnected without sending a response.
 */
void	Server::checkTimeouts()
{
	for (size_t i = 0; i < _pfds.size(); i++) {

		// Not applying time-out logic for CGI Client FDs or Server FDs, skipping
		if (isCgiFd(_pfds[i].fd) || isServerFd(_pfds[i].fd))
			continue;

		auto	it = getRequestByFd(_pfds[i].fd);

		if (it == _clients.end())
			throw std::runtime_error(ERROR_LOG("Could not find request with fd "
									  + std::to_string(_pfds[i].fd)));

		it->checkReqTimeouts();

		if (it->getStatus() == ClientStatus::RecvTimeout
			|| it->getStatus() == ClientStatus::GatewayTimeout) {
			Config const	&conf = matchConfig(*it);

			DEBUG_LOG("Matched config: " + conf.host + " " + conf.serverName + " " + std::to_string(conf.port));
			_responses[_pfds[i].fd].emplace_back(Response(*it, conf));
			sendResponse(i);
		} else if (it->getStatus() == ClientStatus::IdleTimeout
			|| it->getStatus() == ClientStatus::SendTimeout) {
			removeClientFromPollFds(i);
			INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
			cleanupCgi(&(*it));
			_clients.erase(it);
		}
	}
}

/**
 * Checks if current fd is a server (true) or a client (false) fd.
 */
bool	Server::isServerFd(int fd)
{
	auto	it = _serverGroups.begin();

	while (it != _serverGroups.end()) {
		if (it->fd == fd)
			break;
		it++;
	}
	if (it != _serverGroups.end())
		return true;
	return false;
}

/**
 * Loops through _pfds, finding which fd had an event, and whether it's new client or
 * incoming request. If the fd that had a new event is one of the server fds, it's a new client
 * wanting to connect to that server. If it's not a server fd, it is an existing client that has
 * sent data. Thirdly tracks POLLOUT to recognize when server has a response ready to be sent to
 * that client. Fourthly, goes to check all client fds for timeouts.
 */
void	Server::handleConnections()
{
	for (size_t i = 0; i < _pfds.size(); i++) {
		if (_pfds[i].revents & (POLLIN | POLLHUP)) {
			if (isServerFd(_pfds[i].fd))
				handleNewClient(_pfds[i].fd);
			else if (isCgiFd(_pfds[i].fd))
				handleCgiOutput(i);
			else
				handleClientData(i);
		}
		if (_pfds[i].revents & POLLOUT)
			sendResponse(i);
	}
	checkTimeouts();
}

std::vector<Config> const	&Server::getConfigs() const
{
	return _configs;
}

/**
 * At destruction, all file descriptors in _pfds will be closed.
 */
Server::~Server()
{
	for (auto it = _pfds.begin(); it != _pfds.end(); it++)
		close(it->fd);
}

bool	Server::isCgiFd(int fd)
{
	return _cgiFdMap.find(fd) != _cgiFdMap.end();
}

void	Server::handleCgiOutput(size_t &i)
{
	char	buf[CGI_BUF_SIZE];
	int		cgiFd = _pfds[i].fd;

	// If the corresonding FD is not in the CGI map, will remove it from poll fds
	if (_cgiFdMap.find(cgiFd) == _cgiFdMap.end()) {
		ERROR_LOG("CGI fd " + std::to_string(cgiFd) + " not found in map");
		removeClientFromPollFds(i);
		return;
	}

	INFO_LOG("Handling cgi from fd " + std::to_string(_pfds[i].fd));

	ssize_t	bytesRead = read(cgiFd, buf, sizeof(buf)); // Reading data from the CGI client FD
	Request	*req = _cgiFdMap[cgiFd];

	if (bytesRead > 0) { // Successful read, wait for more data
		req->setCgiResult(req->getCgiResult().append(buf, bytesRead)); // Append read buffer to result buffer
		DEBUG_LOG("Read " + std::to_string(bytesRead) + " bytes from CGI (PID: " + std::to_string(req->getCgiPid()) + ")");
		return; // Move to the next poll cycle
	}

	if (bytesRead < 0) { // An error occured, go to next poll cycle (can't use errno)
		ERROR_LOG("CGI read returned -1, deferring to next poll for PID: " + std::to_string(req->getCgiPid()));
		return;
	}

	INFO_LOG("CGI finished execution for fd " + std::to_string(cgiFd));

	int		status;
	pid_t	result = waitpid(req->getCgiPid(), &status, WNOHANG); // Wait for the specific child process

	if (result == 0) { // Child process still running, force kill
		DEBUG_LOG("CGI process still running, killing PID " + std::to_string(req->getCgiPid()));
		kill(req->getCgiPid(), SIGKILL);
		waitpid(req->getCgiPid(), &status, 0);
	} else if (result > 0) {
		INFO_LOG("CGI process " + std::to_string(result) + " exited with status " + std::to_string(status));
	} else {
		ERROR_LOG("Waiting for child failed: " + std::string(strerror(errno)));
	}

	close(cgiFd); // Cleanup CGI fd
	_cgiFdMap.erase(cgiFd); // Client FD from CGI - Request map
	removeClientFromPollFds(i); // Remove CGI Client FD from the POLL list

	if (req->getStatus() != ClientStatus::Invalid) {
		Config const	&conf = matchConfig(*req); // Find config for response

		prepareResponse(*req, conf);

		int	clientFd = req->getFd(); // Find the client FD to enable POLLOUT

		for (auto &pfd : _pfds) {
			if (pfd.fd == clientFd) {
				pfd.events |= POLLOUT;
				break;
			}
		}
		req->setIdleStart();
		req->setSendStart();
	}
}

void	Server::cleanupCgi(Request *req)
{
	// Check if the CGI process is still running
	if (req->getCgiPid() != -1) {
		int		status;
		pid_t	result = waitpid(req->getCgiPid(), &status, WNOHANG);

		if (result == 0) { // Process is still running need to stop
			ERROR_LOG("Forced to kill the child process " + std::to_string(req->getCgiPid()));
			kill(req->getCgiPid(), SIGKILL);
			waitpid(req->getCgiPid(), &status, 0);
		} else if (result == -1 && errno != ECHILD) {
			if (errno == EINTR) {
				ERROR_LOG("Server interrupted while waiting for child: " + std::to_string(req->getCgiPid()));
			} else {
				ERROR_LOG("Unexpected error occurred while waiting for child: " + std::string(strerror(errno)));
			}
		}

		// Remove from POLL_FDS and CGI_FDS
		auto it = _cgiFdMap.begin();

		while (it != _cgiFdMap.end()) {
			if (it->second != req) {
				++it;
				continue;
			}
			int	cgiFD = it->first;

			for (size_t i = 0; i < _pfds.size(); i++) {
				if (_pfds[i].fd == cgiFD) {
					removeClientFromPollFds(i);
					break;
				}
			}
			it = _cgiFdMap.erase(it);
		}
	}
}

void Server::processClientRequest(size_t i, ReqIter it)
{
    Config const &conf = matchConfig(*it);

    if (it->getStatus() == ClientStatus::Error)
    {
        ERROR_LOG("Client fd " + std::to_string(_pfds[i].fd) + " connection dropped: suspicious request");
        removeClientFromPollFds(i);
        INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
        cleanupCgi(&(*it));
        _clients.erase(it);

        return;
    }

    if (it->isHeadersCompleted())
    {
        size_t maxBodySize;
        if (conf.clientMaxBodySize.has_value()) // If there is user defined value for clientMaxBodySize, check against the value
            maxBodySize = conf.clientMaxBodySize.value();
        else // Check against the default clientMaxBodySize value
            maxBodySize = CLIENT_MAX_BODY_SIZE;
        if (it->getContentLength() > maxBodySize)
        {
            it->setResponseCodeBypass(ContentTooLarge);
            it->setStatus(ClientStatus::Invalid);
            ERROR_LOG("Client body size " + std::to_string(it->getContentLength()) + " exceeds the limit " + std::to_string(maxBodySize));
        }
    }

    if (it->isCgiRequest())
    {

        // Lambda function to avoid duplicate code in the error cases below
        auto applySettingsAndPrepareResponse = [it, i, conf, this](std::string msg, ResponseCode resCode)
        {
            ERROR_LOG(msg);
            it->setResponseCodeBypass(resCode);
            it->setStatus(ClientStatus::Invalid);
            prepareResponse(*it, conf);
            _pfds[i].events |= POLLOUT;
            it->setIdleStart();
            it->setSendStart();
        };

        auto routeIt = conf.routes.find("cgi-bin"); // Check if cgi-bin has been routed

        if (routeIt == conf.routes.end())
        { // If cgi-bin isn't set, return
            applySettingsAndPrepareResponse("CGI functionality not enabled!", Forbidden);
            return;
        }

        std::filesystem::path cgiDir = routeIt->second.target; // Extract cgi-bin directory path on the physical disk
        std::string requestedTarget = it->getTarget();
        std::string cgiPrefix = "/cgi-bin/";
        std::string relativePath = requestedTarget.substr(0 + cgiPrefix.length());
        std::filesystem::path path;

        if (relativePath.empty())
        {
            applySettingsAndPrepareResponse("Empty CGI path", Forbidden);
            return;
        }

        path = cgiDir / relativePath; // Buidling the final CGI path
        if (!std::filesystem::exists(path))
        {
            applySettingsAndPrepareResponse("CGI script '" + path.string() + "' does not exist", NotFound);
            return;
        }
        else if (access(path.c_str(), X_OK) == -1)
        {
            applySettingsAndPrepareResponse("CGI script '" + path.string() + "' can't be executed", Forbidden);
            return;
        }

        // Execute the CGI script
        std::pair<pid_t, int> cgiInfo = CgiHandler::execute(path.string(), *it, conf);

        // Error occurred
        if (cgiInfo.first == -1 || cgiInfo.second == -1)
        {
            ERROR_LOG("Error executing CGI script '" + path.string() + "'");
            it->setResponseCodeBypass(InternalServerError);
            it->setStatus(ClientStatus::Invalid);
        }
        else
        {
            INFO_LOG("CGI started with PID " + std::to_string(cgiInfo.first) + " and reading from FD " + std::to_string(cgiInfo.second));
            it->setCgiPid(cgiInfo.first);
            it->setCgiStartTime();
            it->setStatus(ClientStatus::CgiRunning);

            _pfds.push_back({cgiInfo.second, POLLIN, 0}); // Adding CGI read fd to poll list
            _cgiFdMap[cgiInfo.second] = &(*it);
            _pfds[i].events &= ~POLLIN;
            return;
        }
    }

    if (it->getRequestMethod() == RequestMethod::Post && it->boundaryHasValue())
    {
        if (conf.uploadDir.has_value())
        {
            DEBUG_LOG("Handling file upload for client fd " + std::to_string(_pfds[i].fd));
            it->setUploadDir(conf.uploadDir.value());
            it->handleFileUpload();
            if (it->getStatus() == ClientStatus::Error)
            {
                ERROR_LOG("Client fd " + std::to_string(_pfds[i].fd) + " connection dropped: suspicious request");
                removeClientFromPollFds(i);
                INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
                cleanupCgi(&(*it));
                _clients.erase(it);

                return;
            }
        }
        else
        {
            DEBUG_LOG("File uploading is forbidden");
            it->setResponseCodeBypass(Forbidden);
            it->setStatus(ClientStatus::Invalid);
        }
    }

    if (it->getStatus() == ClientStatus::WaitingData)
    {
        INFO_LOG("Waiting for more data to complete partial request");
        return;
    }

    prepareResponse(*it, conf);
    _pfds[i].events |= POLLOUT;
    it->setIdleStart();
    it->setSendStart();
}