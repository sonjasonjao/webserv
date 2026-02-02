#include "Server.hpp"
#include "Log.hpp"
#include "Request.hpp"
#include "Response.hpp"
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
Server::Server(Parser& parser)
{
	signal(SIGINT, handleSignal);
	_configs = parser.getServerConfigs();
	groupConfigs();
}

/**
 * Loops through existing serverGroups and checks whether any of them shares the same
 * IP and port with this current config.
 */
bool	Server::isGroupMember(Config& conf)
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
		if (pollCount < 0)
		{
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
void	Server::handleClientData(size_t& i)
{
	if (_clients.empty())
		throw std::runtime_error(ERROR_LOG("Could not find request with fd "
			+ std::to_string(_pfds[i].fd) + ", clients list empty"));

	auto	it = getRequestByFd(_pfds[i].fd);

	if (it == _clients.end())
		throw std::runtime_error(ERROR_LOG("Could not find request with fd "
			+ std::to_string(_pfds[i].fd)));

	if (it->getStatus() != ClientStatus::WaitingData)
		return;

	char	buf[RECV_BUF_SIZE + 1];

	ssize_t	numBytes = recv(_pfds[i].fd, buf, RECV_BUF_SIZE, 0);

	if (numBytes <= 0)
	{
		if (numBytes == 0)
			INFO_LOG("Client disconnected on fd " + std::to_string(_pfds[i].fd));
		else
			ERROR_LOG("recv: " + std::string(strerror(errno)));

		removeClientFromPollFds(i);

		INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
		_clients.erase(it);
		return;
	}
	buf[numBytes] = '\0';

	INFO_LOG("Received client data from fd " + std::to_string(_pfds[i].fd));
	std::cout << "\n---- Request data ----\n" << buf << "----------------------\n\n";

	it->setIdleStart();
	it->setRecvStart();
	it->processRequest(std::string(buf, numBytes));

	Config const	&conf = matchConfig(*it);

	if (it->getRequestMethod() == RequestMethod::Post && it->boundaryHasValue()) {
		if(conf.uploadDir.has_value()) {
			DEBUG_LOG("Handling file upload for client fd " + std::to_string(_pfds[i].fd));
			it->setUploadDir(conf.uploadDir.value());
			it->handleFileUpload();
		} else {
			DEBUG_LOG("File uploading is forbidden");
			it->setResponseCodeBypass(Forbidden);
			it->setStatus(ClientStatus::Invalid);
		}
	}
	if (it->isHeadersCompleted()) {
		if(conf.clientMaxBodySize.has_value()) {
			// if there is user defined value for clientMaxBodySize check against the value
			if (it->getContentLength() > conf.clientMaxBodySize.value()) {
				it->setResponseCodeBypass(ContentTooLarge);
				it->setStatus(ClientStatus::Invalid);
				ERROR_LOG("Client body size " + std::to_string(it->getContentLength()) + " exceeds the limit " + std::to_string(conf.clientMaxBodySize.value()));
			}
		} else {
			// check against the default clientMaxBodySize value
			if (it->getContentLength() > CLIENT_MAX_BODY_SIZE) {
				it->setResponseCodeBypass(ContentTooLarge);
				it->setStatus(ClientStatus::Invalid);
				ERROR_LOG("Client body size " + std::to_string(it->getContentLength()) + " exceeds the limit " + std::to_string(CLIENT_MAX_BODY_SIZE));
			}
		}
	}

	if (it->getStatus() == ClientStatus::Error) {
		ERROR_LOG("Client fd " + std::to_string(_pfds[i].fd)
			+ " connection dropped: suspicious request");

		removeClientFromPollFds(i);
		INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
		_clients.erase(it);
		return;
	}

	if (it->getStatus() == ClientStatus::WaitingData) {
		INFO_LOG("Waiting for more data to complete partial request");
		return;
	}

	prepareResponse(*it, conf);
	_pfds[i].events |= POLLOUT;
	it->setIdleStart();
	it->setSendStart();
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
	int			fd		= req.getServerFd();
	ServerGroup	*tmp	= nullptr;

	for (auto it = _serverGroups.begin(); it != _serverGroups.end(); it++) {
		if (it->fd == fd) {
			tmp = &(*it);
			break;
		}
	}
	if (tmp == nullptr)
		throw std::runtime_error(ERROR_LOG("Unexpected error in matching request with server config"));
	for (auto it = tmp->configs.begin(); it != tmp->configs.end(); it++) {
		if (it->serverName == req.getHost()) {
			DEBUG_LOG("Matched configuration: " + it->serverName);
			return *it;
		}
	}
	DEBUG_LOG("No matching config, using default: " + tmp->defaultConf->serverName);
	return *(tmp->defaultConf);
}

/**
 * In case of a client that has disconnected itself, or will be disconnected (invalid request,
 * critical error in request, timeout, or keepAlive being false), this function closes its fd
 * and removes it from _pfds.
 */
void	Server::removeClientFromPollFds(size_t& i)
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
void	Server::sendResponse(size_t& i)
{
	auto	it = getRequestByFd(_pfds[i].fd);
	if (it == _clients.end())
		throw std::runtime_error(ERROR_LOG("Could not find request with fd "
			+ std::to_string(_pfds[i].fd)));
	if (it->getStatus() != ClientStatus::ReadyForResponse
		&& it->getStatus() != ClientStatus::RecvTimeout)
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
		_clients.erase(it);
		return;
	}

	it->resetKeepAlive();
	it->setStatus(ClientStatus::WaitingData);
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
		if (!isServerFd(_pfds[i].fd)) {
			auto	it = getRequestByFd(_pfds[i].fd);

			if (it == _clients.end())
				throw std::runtime_error(ERROR_LOG("Could not find request with fd "
					+ std::to_string(_pfds[i].fd)));

			it->checkReqTimeouts();
			if (it->getStatus() == ClientStatus::RecvTimeout) {
				Config const	&conf = matchConfig(*it);

				DEBUG_LOG("Matched config: " + conf.host + " " + conf.serverName + " " + std::to_string(conf.port));
				_responses[_pfds[i].fd].emplace_back(Response(*it, conf));
				sendResponse(i);
			} else if (it->getStatus() == ClientStatus::IdleTimeout
				|| it->getStatus() == ClientStatus::SendTimeout) {
				removeClientFromPollFds(i);
				INFO_LOG("Erasing fd " + std::to_string(it->getFd()) + " from clients list");
				_clients.erase(it);
			}
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
	for (size_t i = 0; i < _pfds.size(); i++)
	{
		if (_pfds[i].revents & (POLLIN | POLLHUP)) {
			if (isServerFd(_pfds[i].fd)) {
				INFO_LOG("Handling new client connecting on fd " + std::to_string(_pfds[i].fd));
				handleNewClient(_pfds[i].fd);
			} else {
				INFO_LOG("Handling client data from fd " + std::to_string(_pfds[i].fd));
				handleClientData(i);
			}
		}
		if (_pfds[i].revents & POLLOUT)
			sendResponse(i);
	}
	checkTimeouts();
}

std::vector<Config> const&	Server::getConfigs() const
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
