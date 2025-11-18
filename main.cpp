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

	ret = getaddrinfo(NULL, "8080", &hints, &servinfo); //IP and port from config file
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
		break;
	}
	if (p == NULL)
		return -1;

	freeaddrinfo(servinfo);

	if (listen(listener, 1000) < 0) //macro for MAX_BACKLOG
		return -1;

	return listener;
}

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "Usage: ./webserv [configuration file]\n";
		return 0;
	}

	int	listener = getListenerSocket();
	if (listener < 0)
	{
		std::cerr << "Server error: listener socket failure\n";
		exit(1);
	}
	Server	starter(getListenerSocket());

	int				fdCount = 1;
	struct pollfd	*pfds = new struct pollfd;
	pfds[0].fd = listener;
	pfds[0].events = POLLIN;

	while (true)
	{
		int	pollCount = poll(pfds, fdCount, -1);
		if (pollCount == -1)
		{
			std::cerr << "Server error: poll\n";
			exit(1);
		}
		//process connections
	}

	return 0;
}
