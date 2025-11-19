#include "Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>

Server::Server(int sockfd)
{
	_listener = sockfd;
}

int	getListenerSocket(void)
{
	int	listener;
	int	yes = 1;
	int	ret;

	struct addrinfo	hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo("127.0.0.1", "8080", &hints, &servinfo); //IP and port from config file
	if (ret != 0)
	{
		std::cerr << "Server error: " << gai_strerror(ret) << '\n';
		exit (1);
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
		if (fcntl(listener, F_SETFL, O_NONBLOCK) < 0)
		{
			std::cerr << "Server error: fcntl failed\n";
			close(listener);
			continue;
		}
		break;
	}
	if (p == NULL)
		return -1;

	freeaddrinfo(servinfo);

	if (listen(listener, 1000) < 0) //macro for MAX_BACKLOG
		return -1;

	std::cout << "Server listening...\n";

	return listener;
}

void	handleNewClient(int listener, struct pollfd*& pfds, int& fdCount)
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
		std::cerr << "Server error: accept failed\n";
		exit(1);
	}
	pfds[fdCount].fd = clientFd;
	pfds[fdCount].events = POLLIN;
	pfds[fdCount].revents = 0;
	std::cout << "Server accepted a new connection with " << clientFd << '\n';

	fdCount++;
}

void	handleClientData(int listener, struct pollfd*& pfds, int& fdCount, int& i)
{
	char	buf[4096];

	int		numBytes = recv(pfds[i].fd, buf, sizeof(buf), 0);
	if (numBytes <= 0)
	{
		if (numBytes == 0)
			std::cout << "Connection closed with " << pfds[i].fd << '\n';
		else
			std::cerr << "Server error: recv failed\n";
		close(pfds[i].fd);
		pfds[i] = pfds[fdCount - 1];
		fdCount--;
		i--; 
	}
	else
	{
		std::cout << "Server got from client " << pfds[i].fd << ": " << buf << '\n';
		//Request const& req = parseRequest(buf);
		//prepareResponse(req);
	}
}

void	processConnections(int listener, struct pollfd*& pfds, int& fdCount)
{
	for (int i = 0; i < fdCount; i++)
	{
		if (pfds[i].revents & (POLLIN | POLLHUP))
		{
			if (pfds[i].fd == listener)
				handleNewClient(listener, pfds, fdCount);
			else
				handleClientData(listener, pfds, fdCount, i);
		}
	}
}

int	main(int argc, char **argv)
{
	// if (argc != 2)
	// {
	// 	std::cout << "Usage: ./webserv [configuration file]\n";
	// 	return 0;
	// }

	int	listener = getListenerSocket();
	if (listener < 0)
	{
		std::cerr << "Server error: listener socket failed\n";
		exit(1);
	}
	Server	starter(listener);

	int				fdCount = 1;
	struct pollfd	*pfds = new struct pollfd;
	pfds[0].fd = listener;
	pfds[0].events = POLLIN;

	while (true)
	{
		int	pollCount = poll(pfds, fdCount, -1);
		if (pollCount == -1)
		{
			std::cerr << "Server error: poll failed\n";
			exit(1);
		}
		processConnections(listener, pfds, fdCount);
	}

	delete pfds;

	return 0;
}