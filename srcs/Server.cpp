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
Server::Server(Parser& parser)
{
	_configs = parser.getServerConfigs();
}

/**
 * This needs to be checked: can we loop through the fds, and how to avoid closing
 * something that's not open?
 */
void	Server::closePfds(void)
{
	for (std::vector<pollfd>::iterator it = _pfds.begin(); it != _pfds.end(); it++)
		close((*it).fd);
}

/**
 * Creates a server socket, binds it to correct address, and starts listening. Still
 * need to check all function call protections, and at which point fcntl() is called.
 */
int	Server::getListenerSocket(std::vector<config_t>::iterator it)
{
	int	listener;
	int	yes = 1;
	int	ret;

	struct addrinfo	hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo((*it).host.c_str(), (std::to_string((*it).port)).c_str(), &hints, &servinfo);
	if (ret != 0)
	{
		ERROR_LOG("Server error: getaddrinfo fail");
		throw std::runtime_error(gai_strerror(ret));
	}

	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
			continue;
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(listener);
			continue;
		}
		// if (fcntl(listener, F_SETFL, O_NONBLOCK) < 0)
		// {
		// 	std::cerr << "Server error: fcntl failed\n";
		// 	close(listener);
		// 	continue;
		// }
		break;
	}
	if (p == NULL)
		return -1;

	freeaddrinfo(servinfo);

	if (listen(listener, 1000) < 0) //macro for MAX_BACKLOG
		return -1;

	INFO_LOG("Server listening...");

	return listener;
}

/**
 * Stores server socket as first element of _pfds (to be moved to function
 * that creates all server sockets), starts poll() loop.
 */
void	Server::run(int listener)
{
	int	fdCount = 1;
	_pfds[0].fd = listener;
	_pfds[0].events = POLLIN;
	_pfds[0].revents = 0;

	while (true)
	{
		int	pollCount = poll(&_pfds[0], fdCount, -1);
		if (pollCount == -1)
		{
			closePfds();
			ERROR_LOG("Server error: poll fail");
			throw std::runtime_error("Server error: poll fail");
		}
		handleConnections(listener, fdCount);
	}
}

/**
 * Accepts new client connection and stores the fd into _pfds.
 */
void	Server::handleNewClient(int listener, int& fdCount)
{
	struct sockaddr_storage	newClient;
	socklen_t				addrLen = sizeof(newClient);
	int						clientFd;

	// if (fdCount == MAX_CONNECTIONS)
	// {
	// 	std::cout << "Unable to accept new connection - reached client limit\n";
	// 	return;
	// }

	clientFd = accept(listener, (struct sockaddr*)&newClient, &addrLen);
	if (clientFd < 0)
	{
		closePfds();
		ERROR_LOG("Server error: failed to accept new connection");
		throw std::runtime_error("Server error: failed to accept new connection");
	}
	_pfds[fdCount].fd = clientFd;
	_pfds[fdCount].events = POLLIN;
	_pfds[fdCount].revents = 0;
	INFO_LOG("Server accepted a new connection with " + std::to_string(clientFd));

	fdCount++;
}

/**
 * Receives data from the client that poll() has recognized ready. Message (= request)
 * will be parsed and response formed.
 */
void	Server::handleClientData(int listener, int& fdCount, int& i)
{
	char	buf[4096];

	int		numBytes = recv(_pfds[i].fd, buf, sizeof(buf), 0);
	if (numBytes <= 0)
	{
		if (numBytes == 0)
			INFO_LOG("Connection closed with " + std::to_string(_pfds[i].fd));
		else
			ERROR_LOG("Server error: recv fail");
		close(_pfds[i].fd);
		_pfds[i] = _pfds[fdCount - 1];
		fdCount--;
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
 * incoming request. Needs modification to cover multiple server sockets.
 *
 * (Could POLLIN and POLLHUP(closed connection) be already distinguished here?)
 */
void	Server::handleConnections(int listener, int& fdCount)
{
	for (int i = 0; i < fdCount; i++)
	{
		if (_pfds[i].revents & (POLLIN | POLLHUP))
		{
			if (_pfds[i].fd == listener)
				handleNewClient(listener, fdCount);
			else
				handleClientData(listener, fdCount, i);
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
