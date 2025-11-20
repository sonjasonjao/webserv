#include "../include/Server.hpp"
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

	std::cout << "Server listening...\n";

	return listener;
}

int	main(int argc, char **argv)
{
	// if (argc != 2)
	// {
	// 	std::cout << "Usage: ./webserv [configuration file]\n";
	// 	return 0;
	// }

	//Parser	parsedConfig(argv[1]);

	int	listener = getListenerSocket(); //do we need multiple listener sockets..?
	if (listener < 0)
	{
		std::cerr << "Server error: failed to create listener socket\n";
		return 1;
	}

	Server	myServer; //config data from parser into this constructor
	myServer.run(listener);

	return 0;
}