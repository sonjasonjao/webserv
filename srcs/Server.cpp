#include "../include/Server.hpp"
#include "../include/Request.hpp"
#include "Response.hpp"
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
 * Looks through existing serverGroups and checks whether any of them shares the same
 * IP and port with this current config.
 */
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
 * Each serverGroup will then have one server (listener) socket.
 */
void	Server::groupConfigs(void)
{
	for (auto it = _configs.begin(); it != _configs.end(); it++)
	{
		if (_serverGroups.empty() || !isGroupMember(*it)) {
			ServerGroup	newServGroup;
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
 * Creates a server socket, binds it to correct address, and starts listening.
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
		if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
		{
			ERROR_LOG("setsockopt: " + std::string(strerror(errno)));
			close(listener);
			continue;
		}
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

	INFO_LOG("Server listening on fd " + std::to_string(listener));

	return listener;
}

/**
 * Loops through serverGroups and creates listener socket for each, stores
 * them into _pfds and stores the fd of the created socket into that serverGroup.
 */
void	Server::createServerSockets()
{
	for (auto it = _serverGroups.begin(); it != _serverGroups.end(); it++)
	{
		int sockfd = getServerSocket(*(it->defaultConf));
		_pfds.push_back({ sockfd, POLLIN, 0 });
		it->fd = sockfd;
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
		int	pollCount = poll(_pfds.data(), _pfds.size(), 1000); //timeout needs to be set
		if (pollCount < 0)
		{
			closePfds();
			throw std::runtime_error(ERROR_LOG("poll: " + std::string(strerror(errno))));
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

	_pfds.push_back({ clientFd, POLLIN | POLLOUT, 0 });
	Request	req(clientFd);
	_clients.push_back(req);
	INFO_LOG("New client accepted, assigned fd " + std::to_string(clientFd));
}

/**
 * Receives data from the client that poll() has recognized ready. Message (= request)
 * will be parsed and response formed.
 */
void	Server::handleClientData(size_t& i)
{
	char	buf[RECV_BUF_SIZE + 1];

	int		numBytes = recv(_pfds[i].fd, buf, RECV_BUF_SIZE, 0);

	if (numBytes <= 0)
	{
		if (numBytes == 0)
			INFO_LOG("Client disconnected on fd " + std::to_string(_pfds[i].fd));
		else
			ERROR_LOG("recv: " + std::string(strerror(errno)));

		INFO_LOG("Closing fd " + std::to_string(_pfds[i].fd));
		close(_pfds[i].fd);

		if (_pfds.size() > (i + 1))
		{
			DEBUG_LOG("Overwriting fd " + std::to_string(_pfds[i].fd) + " with fd " + std::to_string(_pfds[_pfds.size() - 1].fd));
			INFO_LOG("Removing client fd " + std::to_string(_pfds[i].fd) + " from poll list");
			_pfds[i] = _pfds[_pfds.size() - 1];
			_pfds.pop_back();
			i--;
			DEBUG_LOG("Value of i after decrement: " + std::to_string(i));

			return ;
		}

		INFO_LOG("Removing client fd " + std::to_string(_pfds.back().fd) + ", last client");
		_pfds.pop_back();
		i--;
		DEBUG_LOG("Value of i after decrement: " + std::to_string(i));

		return ;
	}

	buf[numBytes] = '\0';
	INFO_LOG("Received client data from fd " + std::to_string(_pfds[i].fd));
	std::cout << "\n---- Request data ----\n" << buf << '\n';

	if (!_clients.empty())
	{
		auto it = _clients.begin();
		while (it != _clients.end())
		{
			if (it->getFd() == _pfds[i].fd)
				break ;
			it++;
		}

		if (it ==_clients.end())
			throw std::runtime_error(ERROR_LOG("Couldn't match client fd to poll fd list"));

		(*it).saveRequest(std::string(buf));

		while (!(*it).isBufferEmpty())
		{
			(*it).handleRequest();

			if ((*it).getKickMe())
			{
				ERROR_LOG("Client fd " + std::to_string(_pfds[i].fd) + " connection dropped: suspicious request");

				INFO_LOG("Closing fd " + std::to_string(_pfds[i].fd));
				close(_pfds[i].fd);

				INFO_LOG("Erasing fd " + std::to_string(_pfds[i].fd) + " from clients list");
				_clients.erase(it);

				if (_pfds.size() > (i + 1))
				{
					DEBUG_LOG("Overwriting fd " + std::to_string(_pfds[i].fd) + " with fd " + std::to_string(_pfds[_pfds.size() - 1].fd));
					INFO_LOG("Removing client fd " + std::to_string(_pfds[i].fd) + " from poll list");
					_pfds[i] = _pfds[_pfds.size() - 1];
					_pfds.pop_back();
					i--;

					return ;
				}

				INFO_LOG("Removing client fd " + std::to_string(_pfds.back().fd) + ", last client");
				_pfds.pop_back();
				i--;
				return ;
			}

			if ((*it).getIsMissingData())
			{
				INFO_LOG("Waiting for more data to complete partial request");

				return ;
			}

			INFO_LOG("Building response to client fd " + std::to_string(_pfds[i].fd));

			//do we need to match here client to config and send that config to Response?

			_responses[_pfds[i].fd].emplace_back(Response(*it));

			// _pfds[i].events |= POLLOUT; we should probably listen to both all the time
		}
	}
}

/**
 * Send protection (disconnect and erase client) missing
 */
void	Server::sendResponse(size_t& i)
{
	auto it = _clients.begin();
	while (it != _clients.end())
	{
		if (it->getFd() == _pfds[i].fd)
			break ;
		it++;
	}

	try
	{
		if (it == _clients.end())
			throw std::runtime_error("Could not find client fd from _clients");
		if (_responses.at(_pfds[i].fd).empty())
			throw std::runtime_error("No responses to send");
	}
	catch(const std::exception& e)
	{
		INFO_LOG(e.what());
		return ;
	}

	auto	&res = _responses[_pfds[i].fd].front();

	INFO_LOG("Sending response to client fd " + std::to_string(_pfds[i].fd));
	send(_pfds[i].fd, res.getContent().c_str(), res.getContent().size(), MSG_DONTWAIT);

	// _pfds[i].events &= ~POLLOUT; we should probably listen to both all the time

	(*it).reset();

	DEBUG_LOG("Keep alive status: " + std::to_string((*it).getKeepAlive()));
	if (!(*it).getKeepAlive())
	{
		INFO_LOG("Closing fd " + std::to_string(_pfds[i].fd));
		close(_pfds[i].fd);

		INFO_LOG("Erasing fd " + std::to_string(_pfds[i].fd) + " from clients list");
		_clients.erase(it);

		if (_pfds.size() > (i + 1))
		{
			DEBUG_LOG("Overwriting fd " + std::to_string(_pfds[i].fd) + " with fd " + std::to_string(_pfds[_pfds.size() - 1].fd));
			INFO_LOG("Removing client fd " + std::to_string(_pfds[i].fd) + " from poll list");
			_pfds[i] = _pfds[_pfds.size() - 1];
			_pfds.pop_back();
			i--;

			return ;
		}

		INFO_LOG("Removing client fd " + std::to_string(_pfds.back().fd) + ", last client");
		_pfds.pop_back();
		i--;
	}
	(*it).resetKeepAlive();
	_responses[_pfds[i].fd].pop_front();
}

/**
 * Loops through _pfds, finding which fd had an event, and whether it's new client or
 * incoming request. If the fd that had a new event is one of the server fds, it's a new client
 * wanting to connect to that server. If it's not a server fd, it is an existing client that has
 * sent data.
 *
 * We probably need to implement tracking of POLLOUT here, and handle the sending through
 * a separate process, as it now is by just calling send() once in handleClientData().
 */
void	Server::handleConnections(void)
{
	for (size_t i = 0; i < _pfds.size(); i++)
	{
		if (_pfds[i].revents & (POLLIN | POLLHUP))
		{
			auto	it = _serverGroups.begin();
			while (it != _serverGroups.end())
			{
				if (it->fd == _pfds[i].fd)
					break ;
				it++;
			}

			if (it != _serverGroups.end())
			{
				INFO_LOG("Handling new client connecting on fd " + std::to_string(it->fd));
				handleNewClient(it->fd);
			}
			else
			{
				INFO_LOG("Handling client data from fd " + std::to_string(_pfds[i].fd));
				handleClientData(i);
			}
		}
		if (_pfds[i].revents & POLLOUT)
			sendResponse(i);
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
