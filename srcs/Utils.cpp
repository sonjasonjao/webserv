#include "Utils.hpp"
#include <cctype>
#include <sstream>
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

/**
 * NOTE: Assuming that the input string is trimmed in advance
 */
bool	isValidIPv4(std::string_view str)
{
	if (str.empty())
		return false;

	unsigned int	dots	= 0;
	unsigned int	octets	= 0;
	unsigned int	octet	= 0;

	// Doing this to only go through the string once, and practice some old
	// fashioned parsing
	size_t	i = -1;
	while (++i < str.size()) {
		if (!(str[i] == '.' || isdigit(str[i])))
			return false;
		if (str[i] == '.') {
			++dots;
			if (dots > 3)
				return false;
			// '.' can't be the first or last character and has to be followed by a digit
			if (i == 0 || (i == str.size() - 1) || !isdigit(str[i + 1]))
				return false;
			continue;
		}
		++octets;
		if (octets > 4)		// Can't have more than 4 octets in an IPv4
			return false;
		octet = 0;
		while (i < str.size() && std::isdigit(str[i])) {	// Calculate & check size of resulting octet
			octet = octet * 10 + str[i] - '0';
			if (octet > 255)
				return false;
			++i;
		}
		--i;	// Move back to the last number so that the while incrementation doesn't skip a character
	}
	if (dots != 3 || octets != 4)
		return false;

	return true;
}

#ifdef TEST

#include <iostream>

int	main()
{
	std::cout << getIMFFixdate() << "\n";
	std::cout << isValidIPv4("1.1.1.1") << "\n";
	std::cout << isValidIPv4("1.1.1") << "\n";
	std::cout << isValidIPv4("1.1.1.") << "\n";
	std::cout << isValidIPv4(".1.1.1") << "\n";
	std::cout << isValidIPv4("1.a.1.1") << "\n";
	std::cout << isValidIPv4("255.255.255.255.255") << "\n";
	std::cout << isValidIPv4("255.255.255.255") << "\n";
	std::cout << isValidIPv4("256.255.255.255") << "\n";
	std::cout << isValidIPv4("") << "\n";
	return 0;
}

#endif
