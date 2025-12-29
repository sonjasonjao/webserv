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
#include <cstring>
#include <vector>
#include <poll.h>


/**
 * This is a simple client program to test different cases, such as sending partial requests,
 * chunked requests, etc.
 *
 * In the poll loop, i and j manually track the duration of the loop so the program breaks out
 * after sending and receiving as expected.
 */
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
	if (getaddrinfo(argv[1], argv[2], &hints, &res) < 0)
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
	std::vector<pollfd>	pfd;
	pollfd	tmp;
	pfd.push_back({ sockfd, POLLOUT, 0 });
	char msg[61] =
	"GET / HTTP/1.0\r\nConnection: Keep-alive\r\nHost: www.test.com\r\n";
	char msg2[71] =
	"Transfer-encoding: Chunked\r\n\r\n9\r\nThis is b\r\n0F\r\nThis is another\r\n0\r\n\r\n";
	char msg3[72] =
	"POST / HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-length: 15\r\n\r\nWe test this!!!";
	int		i = 0;
	int		j = 0;
	ssize_t	ret = 0;
	while (true)
	{
		if (poll(pfd.data(), pfd.size(), 1000) < 0) {
			perror("poll");
			break ;
		}
		if ((pfd[0].revents & POLLOUT) && i < 3) {
			if (i == 0)
				ret = send(sockfd, msg, strlen(msg), 0);
			if (i == 1) {
				ret = send(sockfd, msg2, strlen(msg2), 0);
				pfd[0].events |= POLLIN;
			}
			if (i == 2) {
				ret = send(sockfd, msg3, strlen(msg3), 0);
				pfd[0].events |= POLLIN;
			}
			if (ret < 0) {
				std::cerr << "Error: send failure\n";
				break ;
			}
			i++;
			std::cout << "Sent\n";
			usleep(1000);
		}
		else if ((pfd[0].revents & POLLIN) && j < 1) {
			ssize_t	numBytes;
			char	buf[1025];
			numBytes = recv(sockfd, buf, 1024, 0);
			if (numBytes == -1) {
				perror("recv");
				exit(1);
			}
			if (numBytes == 0) {
				std::cout << "Server closed the connection\n";
				break ;
			}
			buf[numBytes] = '\0';
			std::cout << "Received: '" << buf << "'\n";
			pfd[0].events &= ~POLLIN;
			j++;
			usleep(1000);
		}
	}
	close(sockfd);

	return 0;
}
