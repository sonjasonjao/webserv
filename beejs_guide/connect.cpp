#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "Log.hpp"

int	main(int argc, char *argv[])
{
	struct addrinfo	hints;
	struct addrinfo	*res;
	struct addrinfo	*p;
	int				status;
	char			ipstr[INET6_ADDRSTRLEN];

	bzero(&hints, sizeof(hints));
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	if ((status = getaddrinfo("www.example.com", "3490", &hints, &res)) != 0) {
		ERROR_LOG("getaddrinfo: " + std::string(gai_strerror(status)));
		return EXIT_FAILURE;
	}

	int	sockfd;
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		ERROR_LOG("socket: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}

	if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		ERROR_LOG("connect: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}

	return 0;
}
