#include <iostream>
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

	if (argc != 2) {
		std::cerr << "Usage: ./showip <hostname>\n";
		return EXIT_FAILURE;
	}

	bzero(&hints, sizeof(hints));
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	if ((status = getaddrinfo(argv[1], NULL, &hints, &res)) != 0) {
		ERROR_LOG("getaddrinfo: " + std::string(gai_strerror(status)));
		return EXIT_FAILURE;
	}

	std::cout << "IP addresses for " << argv[1] << ":\n\n";

	for (p = res; p != nullptr; p = p->ai_next) {
		void				*addr;
		std::string			ipver;
		struct sockaddr_in	*ipv4;
		struct sockaddr_in6	*ipv6;

		if (p->ai_family == AF_INET) {
			ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);
			ipver = "IPv4";
		} else {
			ipv6 = (struct sockaddr_in6 *)p->ai_addr;
			addr = &(ipv6->sin6_addr);
			ipver = "IPv6";
		}
		inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
		std::cout << "	" << ipver << ": " << ipstr << "\n";
	}

	freeaddrinfo(res);
	return 0;
}
