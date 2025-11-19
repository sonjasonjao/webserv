#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <fcntl.h>

int main(int argc, char **argv)
{
	if (argc != 3)
		return 0 ;

	struct addrinfo hints;
	struct addrinfo* res;
	int sockfd = -1;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(argv[1], "8080", &hints, &res) < 0)
	{
		std::cerr << "Error: client getaddrinfo failed\n";
		exit (1);
	}

	for (struct addrinfo* p = res; p != NULL; p = p->ai_next)
	{
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == -1)
			continue ;
		if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)
		{
			std::cerr << "Client error: fcntl failed\n";
			exit(1);
		}
		connect(sockfd, p->ai_addr, p->ai_addrlen);
		break ;
	}
	freeaddrinfo(res);
	char* msg = argv[2];
	send(sockfd, msg, sizeof(msg), 0);
	close(sockfd);

	return 0;
}
