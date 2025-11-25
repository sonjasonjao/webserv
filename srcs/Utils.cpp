#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <clocale>

std::string	getIMFFixdate()
{
	auto const			now			= std::chrono::system_clock::now();
	auto const			nowTimeT	= std::chrono::system_clock::to_time_t(now);
	std::stringstream	timeStream;

	std::setlocale(LC_ALL, "C");
	timeStream << std::put_time(std::gmtime(&nowTimeT), "%a, %d %b %Y %H:%M:%S GMT");

	return timeStream.str();
}

#ifdef TEST

#include <iostream>

int	main()
{
	std::cout << getIMFFixdate();
	return 0;
}

#endif
