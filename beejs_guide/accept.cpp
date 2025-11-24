#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "Log.hpp"

#define MYPORT	"3490"
#define BACKLOG	10

int	main()
{
	struct addrinfo	hints;
	struct addrinfo	*res;

	bzero(&hints, sizeof(hints));
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= AI_PASSIVE;

	int	status;
	if ((status = getaddrinfo(NULL,MYPORT, &hints, &res)) != 0) {
		ERROR_LOG("getaddrinfo: " + std::string(gai_strerror(status)));
		return EXIT_FAILURE;
	}

	int	sockfd;
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		ERROR_LOG("socket: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		ERROR_LOG("bind: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}

	if (listen(sockfd, BACKLOG) == -1) {
		ERROR_LOG("listen: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}

	struct sockaddr_storage	their_addr;
	socklen_t				addr_size	= sizeof(their_addr);

	int	new_fd;
	if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1) {
		ERROR_LOG("accept: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}
	return 0;
}
