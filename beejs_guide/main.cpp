#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include "Log.hpp"
#include <iostream>
#include <cstring>

void	endianTest()
{
	uint32_t	num		= 0xa1b2c3d4;
	uint8_t		*ptr	= (uint8_t *)&num;

	printf("First byte: %x\n", *ptr);

	if (*ptr == 0xa1)
		std::cout << "Big endian\n";
	else
		std::cout << "Little endian\n";
}

bool	isBigEndian()
{
	uint32_t	num		= 0xa1b2c3d4;
	uint8_t		*ptr	= (uint8_t *)&num;

	return (*ptr == (num >> 24));
}

bool	isLittleEndian()
{
	uint32_t	num		= 0xa1b2c3d4;
	uint8_t		*ptr	= (uint8_t *)&num;

	return (*ptr == (num & 0xff));
}

int	main()
{
	// if (isLittleEndian())
	// 	endianTest();
	// ERROR_LOG("This is an error message");
	// INFO_LOG("This is an info message");
	// DEBUG_LOG("This is a debug message");
	int				status;
	struct addrinfo	hints;
	struct addrinfo	*servinfo;
	struct addrinfo	*p;

	bzero(&hints, sizeof(hints));
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= AI_PASSIVE;

	if ((status = getaddrinfo(NULL, "9876", &hints, &servinfo)) != 0) {
		ERROR_LOG("getaddrinfo fail: " + std::string(gai_strerror(status)));
		return (EXIT_FAILURE);
	}

	freeaddrinfo(servinfo);
	return 0;
}
