#include "Log.hpp"
#include <iostream>

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
	if (isLittleEndian())
		endianTest();
	ERROR_LOG("This is an error message");
	INFO_LOG("This is an info message");
	DEBUG_LOG("This is a debug message");
	return 0;
}
