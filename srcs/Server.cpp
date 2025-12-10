#include "../include/Server.hpp"
#include "../include/Request.hpp"
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
 *
 * For now, we manually fetch the three configs that test.json/servergrups.json has, and move on
 * to group the configs.
 */
Server::Server(Parser& parser)
{
	//_configs = parser.getConfigs();
	Config tmp = parser.getServerConfig(0);
	_configs.push_back(tmp);
	tmp = parser.getServerConfig(1);
	_configs.push_back(tmp);
	tmp = parser.getServerConfig(2);
	_configs.push_back(tmp);
	groupConfigs();
}

/**
 * Do we need this?
 */
Server::Server(Server const& obj)
{
	_configs = obj._configs;
	_pfds = obj._pfds;
	_serverGroups = obj._serverGroups;
}

bool	Server::isGroupMember(Config& conf)
{
	for (auto it = _serverGroups.begin(); it != _serverGroups.end(); it++)
	{
		if (it->defaultConf->host == conf.host
			&& it->defaultConf->ports.front() == conf.ports.front()) {
			it->configs.push_back(conf);
			return true;
		}
	}
	return false;
}

/**
 * Groups configs so that all configs in one group have the same IP and the same port.
 * Each listenergroup will then have one server (listener) socket.
 */
void	Server::groupConfigs(void)
{
	for (auto it = _configs.begin(); it != _configs.end(); it++)
	{
		if (_serverGroups.empty() || !isGroupMember(*it)) {
			ListenerGroup	newServGroup;
			newServGroup.fd = -1;
			newServGroup.configs.push_back(*it);
			newServGroup.defaultConf = &(*it);
			_serverGroups.push_back(newServGroup);
		}
	}
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
int	Server::getServerSocket(Config conf)
{
	int	listener;
	int	yes = 1;
	int	ret;

	struct addrinfo	hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(conf.host.c_str(), (std::to_string(conf.ports.at(0))).c_str(),
		&hints, &servinfo);
	if (ret != 0)
		throw std::runtime_error(ERROR_LOG("getaddrinfo: " + std::string(gai_strerror(ret))));

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
		throw std::runtime_error(ERROR_LOG("could not create server socket(s)"));

	freeaddrinfo(servinfo);

	if (listen(listener, MAX_PENDING) < 0)
		throw std::runtime_error(ERROR_LOG("listen: " + std::string(strerror(errno))));

	INFO_LOG("Server listening on " + std::to_string(listener));

	return listener;
}

/**
 * Loops through server configurations and creates listener socket for each, stores
 * them into _pfds and stores the pair of fd and config into _fdToConfig.
 */
void	Server::createServerSockets()
{
	for (auto it = _serverGroups.begin(); it != _serverGroups.end(); it++)
	{
		int sockfd = getServerSocket(*(it->defaultConf));
		_pfds.push_back({ sockfd, POLLIN, 0 });
		it->fd = sockfd; //making a copy of each config not really efficient
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
		int	pollCount = poll(&_pfds[0], _pfds.size(), 1000); //timeout needs to be set
		if (pollCount < 0)
		{
			closePfds();
			throw std::runtime_error(ERROR_LOG("poll: " + std::string(strerror(errno))));
		}
		// DEBUG_LOG("pollCount: " + std::string(std::to_string(pollCount)));
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
		throw std::runtime_error(ERROR_LOG("accept: " + std::string(strerror(errno))));
	}
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientFd);
		closePfds();
		throw std::runtime_error(ERROR_LOG("fcntl: " + std::string(strerror(errno))));
	}
	_pfds.push_back({ clientFd, POLLIN, 0 });
	Request	req(clientFd);
	_clients.push_back(req);
	INFO_LOG("Server accepted a new connection with " + std::to_string(clientFd));
}

/**
 * Receives data from the client that poll() has recognized ready. Message (= request)
 * will be parsed and response formed.
 *
 * Probably will need to differentiate _isValid and _kickClient, so if e.g. a chunked request
 * has hex size that does not match the actual size, the client must be disconnected altogether.
 */
void	Server::handleClientData(size_t& i)
{
	char	buf[RECV_BUF_SIZE + 1];

	int		numBytes = recv(_pfds[i].fd, buf, RECV_BUF_SIZE, 0);
	if (numBytes <= 0)
	{
		if (numBytes == 0)
			INFO_LOG("Connection closed with " + std::to_string(_pfds[i].fd));
		else
			ERROR_LOG("recv: " + std::string(strerror(errno)));
		DEBUG_LOG("Closing fd " + std::to_string(_pfds[i].fd));
		close(_pfds[i].fd);
		if (_pfds.size() > (i + 1)) {
			_pfds[i] = _pfds[_pfds.size() - 1];
			i--;
		}
	}
	else
	{
		buf[numBytes] = '\0';
		INFO_LOG("Server received data from client " + std::to_string(_pfds[i].fd));
		std::cout << buf << '\n';
		if (!_clients.empty()) {
			auto it = _clients.begin();
			while (it != _clients.end()) {
				if (it->getFd() == _pfds[i].fd)
					break ;
				it++;
			}
			if (it ==_clients.end())
				ERROR_LOG("Unexpected error in finding client fd"); //bad messaging
			(*it).saveRequest(std::string(buf));
			while (!(*it).isBufferEmpty()) {
				(*it).handleRequest();
				if ((*it).getIsMissingData()) {
					INFO_LOG("Waiting for more data to complete partial request");
					return ;
				}
				if (!(*it).getIsValid()) {
					ERROR_LOG("Invalid HTTP request");
					return ;
				}
				(*it).reset();
				//build and send response
			}
			if (!(*it).getKeepAlive()) {
				close(_pfds[i].fd);
				if (_pfds.size() > (i + 1)) {
					_pfds[i] = _pfds[_pfds.size() - 1];
					i--;
				}
			}
		}
	}
}

/**
 * Loops through _pfds, finding which fd triggered poll, and whether it's new client or
 * incoming request. If the fd that had a new event is one of the server fds, it's a new client
 * wanting to connect to that server. If it's not a server fd, it is an existing client that has
 * sent data.
 */
void	Server::handleConnections(void)
{
	for (size_t i = 0; i < _pfds.size(); i++)
	{
		if (_pfds[i].revents & (POLLIN | POLLHUP)) {
			auto	it = _serverGroups.begin();
			while (it != _serverGroups.end()) {
				if (it->fd == _pfds[i].fd)
					break ;
				it++;
			}
			if (it != _serverGroups.end())
				handleNewClient(it->fd);
			else
				handleClientData(i);
		}
	}
}

std::vector<Config> const&	Server::getConfigs() const
{
	return _configs;
}

Server::~Server()
{
	closePfds();
}
