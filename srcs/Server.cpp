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
#include <sys/epoll.h>

/**
 * At construction, _configs will be fetched from parser.
 */
Server::Server(Parser& parser)
{
	config_t tmp;
	parser.getServerConfig("test", tmp);
	_configs.push_back(tmp);
}

Server::Server(Server const& obj)
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
int	Server::getServerSocket(config_t conf)
{
	int	listener;
	int	yes = 1;
	int	ret;

	struct addrinfo	hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(conf.host.c_str(), (std::to_string(conf.port)).c_str(),
		&hints, &servinfo);
	if (ret != 0)
	{
		ERROR_LOG("getaddrinfo: " + std::string(gai_strerror(ret)));
		throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(ret)));
	}

	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
		{
			ERROR_LOG("socket: " + std::string(strerror(errno)));
			continue;
		}
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			ERROR_LOG("bind: " + std::string(strerror(errno)));
			close(listener);
			continue;
		}
		if (fcntl(listener, F_SETFL, O_NONBLOCK) < 0)
		{
			ERROR_LOG("fcntl: " + std::string(strerror(errno)));
			close(listener);
			continue;
		}
		break;
	}
	if (p == NULL)
	{
		ERROR_LOG("could not create server socket(s)");
		throw std::runtime_error("could not create server socket(s)");
	}

	freeaddrinfo(servinfo);

	if (listen(listener, MAX_PENDING) < 0)
	{
		ERROR_LOG("listen: " + std::string(strerror(errno)));
		throw std::runtime_error("listen: " + std::string(strerror(errno)));
	}

	INFO_LOG("Server listening on " + std::to_string(listener));

	return listener;
}

/**
 * Loops through server configurations and creates listener socket for each, stores
 * them into _pfds and stores the pair of fd and config into _serverFds.
 */
void	Server::createServerSockets()
{
	for (auto it = _configs.begin(); it != _configs.end(); it++)
	{
		int sockfd = getServerSocket(*it);
		_pfds.push_back({ sockfd, POLLIN, 0 });
		_serverFds[sockfd] = *it; //making a copy of each config not really efficient
	}
}

/**
 * Calls getServerSockets() to create listener sockets, starts poll() loop.
 */
void	Server::run(void)
{
	createServerSockets();
	while (true)
	{
		int	pollCount = poll(&_pfds[0], _pfds.size(), 3000);
		if (pollCount < 0)
		{
			closePfds();
			ERROR_LOG("poll: " + std::string(strerror(errno)));
			throw std::runtime_error("poll: " + std::string(strerror(errno)));
		}
		DEBUG_LOG("pollCount: " + std::string(std::to_string(pollCount)));
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

	// if (_pfds.size() == MAX_CLIENTS) //where do we get this from? ERROR or INFO_LOG?
	// {
	// 	ERROR_LOG("failed to accept new client connection - max clients reached");
	// 	std::cout << "Failed to accept new connection - max clients reached\n";
	// 	return;
	// }

	clientFd = accept(listener, (struct sockaddr*)&newClient, &addrLen);
	if (clientFd < 0)
	{
		closePfds();
		ERROR_LOG("accept: " + std::string(strerror(errno)));
		throw std::runtime_error("accept: " + std::string(strerror(errno)));
	}
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientFd);
		closePfds();
		ERROR_LOG("fcntl: " + std::string(strerror(errno)));
		throw std::runtime_error("fcntl: " + std::string(strerror(errno)));
	}
	_pfds.push_back({ clientFd, POLLIN, 0 });
	INFO_LOG("Server accepted a new connection with " + std::to_string(clientFd));
}

/**
 * Receives data from the client that poll() has recognized ready. Message (= request)
 * will be parsed and response formed.
 *
 * Still missing handling of partial request - need to store data in tmp buffer and
 * join two or more received data blocks from separate poll rounds. How to catch if a
 * request is not complete?
 */
void	Server::handleClientData(size_t& i)
{
	char	buf[4096];

	int		numBytes = recv(_pfds[i].fd, buf, sizeof(buf), 0);
	if (numBytes <= 0)
	{
		if (numBytes == 0)
			INFO_LOG("Connection closed with " + std::to_string(_pfds[i].fd));
		else
			ERROR_LOG("recv: " + std::string(strerror(errno)));
		close(_pfds[i].fd);
		_pfds[i] = _pfds[_pfds.size() - 1];
		i--;
	}
	else
	{
		INFO_LOG("Server received data from client " + std::to_string(_pfds[i].fd));
		std::cout << buf << '\n';
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
	for (size_t i = 0; i < _pfds.size(); i++)
	{
		if (_pfds[i].revents & (POLLIN | POLLHUP))
		{
			auto pos = _serverFds.find(_pfds[i].fd);
			if (pos != _serverFds.end())
				handleNewClient(pos->first);
			else
			{
				handleClientData(i);
				break ;
			}
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
