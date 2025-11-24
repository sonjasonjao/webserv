#include "../include/Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cstring>

/**
 * At construction, _configs will be fetched from parser.
 */
Server::Server(Parser const& parser)
{
	_configs = parser.getServerConfigs();
}

Server::Server(Server& const obj)
{
	_configs = obj._configs;
	_pfds = obj._pfds;
	_serverFds = obj._serverFds;
}

/**
 * This needs to be checked: can we loop through the fds, and how to avoid closing
 * something that's not open?
 */
void	Server::closePfds(void)
{
	for (auto it = _pfds.begin(); it != _pfds.end(); it++)
		close((*it).fd);
}

/**
 * Creates a server socket, binds it to correct address, and starts listening. Still
 * need to check all function call protections, and at which point fcntl() is called.
 */
int	Server::getListenerSocket(config_t conf)
{
	int	listener;
	int	yes = 1;
	int	ret;

	struct addrinfo	hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(conf.host.c_str(), (std::to_string(conf.port)).c_str(), &hints, &servinfo);
	if (ret != 0)
	{
		ERROR_LOG("getaddrinfo() fail");
		throw std::runtime_error(gai_strerror(ret));
	}

	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
		{
			ERROR_LOG("socket() fail");
			continue;
		}
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			ERROR_LOG("bind() fail");
			close(listener);
			continue;
		}
		if (fcntl(listener, F_SETFL, O_NONBLOCK) < 0)
		{
			ERROR_LOG("fcntl fail");
			close(listener);
			continue;
		}
		break;
	}
	if (p == NULL)
		return -1;

	freeaddrinfo(servinfo);

	if (listen(listener, 1000) < 0) //macro for MAX_PENDING
		return -2;

	INFO_LOG("Server listening...");

	return listener;
}

/**
 * Loops through server configurations and creates listener socket for each, stores
 * them into _pfds and stores the pair of fd and config into _serverFds.
 */
void	Server::getServerSockets()
{
	for (auto it = _configs.begin(); it != _configs.end(); it++)
	{
		int sockfd = getListenerSocket(*it);
		if (sockfd == -1)
			throw std::runtime_error("Server error: could not create server socket(s)");
		if (sockfd == -2)
			throw std::runtime_error("Server error: listen fail");
		size_t	i = _pfds.size();
		_pfds[i].fd = sockfd;
		_pfds[i].events = POLLIN;
		_pfds[i].revents = 0;
		_serverFds[sockfd] = *it; //making a copy of each config not really efficient
	}
}

/**
 * Calls getServerSockets() to create listener sockets, starts poll() loop.
 */
void	Server::run(void)
{
	getServerSockets();
	while (true)
	{
		int	pollCount = poll(&_pfds[0], _pfds.size(), -1);
		if (pollCount == -1)
		{
			closePfds();
			ERROR_LOG("poll() fail");
			throw std::runtime_error("Server error: poll fail");
		}
		handleConnections();
	}
}

/**
 * Accepts new client connection and stores the fd into _pfds.
 */
void	Server::handleNewClient(int listener)
{
	struct sockaddr_storage	newClient;
	socklen_t				addrLen = sizeof(newClient);
	int						clientFd;

	// if (_pfds.size() == MAX_CONNECTIONS) //or member attribute from parser?
	// {
	// 	std::cout << "Unable to accept new connection - reached client limit\n";
	// 	return;
	// }

	clientFd = accept(listener, (struct sockaddr*)&newClient, &addrLen);
	if (clientFd < 0)
	{
		closePfds();
		ERROR_LOG("accept() fail");
		throw std::runtime_error("Server error: failed to accept new connection");
	}
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientFd);
		closePfds();
		ERROR_LOG("fcntl() fail");
		throw std::runtime_error("Error: fcntl() to set client non-blocking");
	}
	size_t	i = _pfds.size();
	_pfds[i].fd = clientFd;
	_pfds[i].events = POLLIN;
	_pfds[i].revents = 0;
	INFO_LOG("Server accepted a new connection with " + std::to_string(clientFd));
}

/**
 * Receives data from the client that poll() has recognized ready. Message (= request)
 * will be parsed and response formed.
 */
void	Server::handleClientData(int& i)
{
	char	buf[4096];

	int		numBytes = recv(_pfds[i].fd, buf, sizeof(buf), 0);
	if (numBytes <= 0)
	{
		if (numBytes == 0)
			INFO_LOG("Connection closed with " + std::to_string(_pfds[i].fd));
		else
			ERROR_LOG("recv() fail");
		close(_pfds[i].fd);
		_pfds[i] = _pfds[_pfds.size() - 1];
		i--;
	}
	else
	{
		INFO_LOG("Server received data from client " + std::to_string(_pfds[i].fd));
		//Request const& req = parseRequest(buf);
		//prepareResponse(req);
	}
}

/**
 * Loops through _pfds, finding which fd triggered poll, and whether it's new client or
 * incoming request.
 *
 * (Could POLLIN and POLLHUP(closed connection) be already distinguished here?)
 */
void	Server::handleConnections(void)
{
	for (int i = 0; i < _pfds.size(); i++)
	{
		if (_pfds[i].revents & (POLLIN | POLLHUP))
		{
			auto pos = _serverFds.find(_pfds[i].fd);
			if (pos != _serverFds.end())
				handleNewClient(pos->first);
			else
				handleClientData(i);
		}
	}
}

std::vector<config_t> const&	Server::getConfigs() const
{
	return _configs;
}

Server::~Server()
{
	closePfds();
}
