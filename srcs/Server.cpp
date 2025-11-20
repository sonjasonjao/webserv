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
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

Server::Server()
{
	//name, id and port from the config file after parsing, what else is needed?
	_pfds = new struct pollfd;
}

void sigchld_handler(int s)
{
	(void)s;
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}

void	Server::run(int listener)
{
	int	fdCount = 1;
	_pfds[0].fd = listener;
	_pfds[0].events = POLLIN;

	while (true)
	{
		int	pollCount = poll(_pfds, fdCount, -1);
		if (pollCount == -1)
		{
			std::cerr << "Server error: poll failed\n";
			delete _pfds;
			exit(1);
		}
		processConnections(listener, fdCount);
	}
}

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
	struct sigaction sa;

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}
	clientFd = accept(listener, (struct sockaddr*)&newClient, &addrLen);
	if (clientFd < 0)
	{
		std::cerr << "server error: accept failed\n";
		exit(1);
	}
	_pfds[fdCount].fd = clientFd;
	_pfds[fdCount].events = POLLIN;
	_pfds[fdCount].revents = 0;
	std::cout << "server accepted a new connection with " << clientFd << '\n';

	fdCount++;
}

void	Server::handleClientData(int listener, int& fdCount, int& i)
{
	char	buf[4096];

	int		numBytes = recv(_pfds[i].fd, buf, sizeof(buf), 0);
	if (numBytes <= 0)
	{
		if (numBytes == 0)
			std::cout << "connection closed with " << _pfds[i].fd << '\n';
		else
			std::cerr << "server error: recv failed\n";
		close(_pfds[i].fd);
		_pfds[i] = _pfds[fdCount - 1];
		fdCount--;
		i--; 
	}
	else
	{
		std::cout << "server received from client " << _pfds[i].fd << ": " << buf << '\n';
		//Request const& req = parseRequest(buf);
		//prepareResponse(req);
	}
}

void	Server::processConnections(int listener, int& fdCount)
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

Server::~Server()
{
	for (int i = 0; _pfds[i].fd != -1; i++)
		close(_pfds[i].fd);
	delete _pfds;
}