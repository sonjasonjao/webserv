#include "Utils.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <clocale>
#include <filesystem>
#include <vector>
#include <algorithm>

/**
 * The IMF fixdate is the preferred format for HTTP timestamps
 *
 * @return	String containing the current system time in IMF fixdate format
 */
std::string	getImfFixdate()
{
	auto const			now			= std::chrono::system_clock::now();
	auto const			nowTimeT	= std::chrono::system_clock::to_time_t(now);
	std::stringstream	timeStream;

	std::setlocale(LC_ALL, "C");
	timeStream << std::put_time(std::gmtime(&nowTimeT), "%a, %d %b %Y %H:%M:%S GMT");

	return timeStream.str();
}

/**
 * NOTE:	Creates empty members for cases where two delmiters are next to each other,
 *			or when starting or ending with a delimiter
 *
 * @return	string vector containing the parts separated by delim
 */
std::vector<std::string>	split(std::string_view sv, std::string_view delim = " ")
{
	std::vector<std::string>	res		= {};
	std::string					parse	= std::string(sv);
	std::string					token	= "";
	size_t						pos		= 0;

	while (!parse.empty()) {
		pos		= parse.find(delim);
		token	= parse.substr(0, pos);

		res.push_back(token);
		if (pos == std::string::npos)
			parse = "";
		else
			parse.erase(0, pos + delim.length());
	}

	if (sv.substr(sv.length() - delim.length()) == delim)
			res.push_back("");

	return res;
}

/**
 * @return	string vector containing the url parts split by '/'
 */
std::vector<std::string>	splitUrl(std::string_view url)
{
	return split(url, "/");
}

/**
 * Function checks if the string_view parameter contains a valid IPv4 address.
 * NOTE: Assuming that the input string is trimmed in advance
 */
bool	isValidIPv4(std::string_view sv)
{
	if (sv.empty())
		return false;

	std::vector<std::string>	octets = split(sv, ".");

	if (octets.size() != 4)
		return false;

	for (auto const &oct : octets) {
		if (oct.empty())
			return false;
		if (!std::all_of(oct.begin(), oct.end(), isdigit))
			return false;
		try {
			if (std::stoi(oct) > 255)
			return false;
		} catch (std::exception const &e) {
			return false;
		}
	}

	return true;
}

/**
 * Need a way to get the directory that our html content is saved at
 * NOTE: Do we need to pass the whole server info or a specifically matched config?
 */
std::string	getRoot()
{
	return "/";
}

/**
 * NOTE: Was there a boolean for the behaviour if we give the default index.html for a dir?
 */
bool	resourceExists(std::string_view url)
{
	if (url.empty())
		return false;

	std::string root = getRoot();	// Probably need to match server config to get root folder for content?
	std::string	path;

	if (url[0] == '/')
		path = url.substr(1);
	if (root.back() == '/')
		root.pop_back();

	path = root + "/" + path;

	return std::filesystem::exists(path);
}

/**
 * Function to determine if a URL contains forbidden symbols or is otherwise
 * incorrectly written.
 */
bool	urlIsMisformed(std::string_view url)
{
	return false;
}

/**
 * Check whether a correct URL tries to access files beyond root
 *
 * NOTE: Assumes valid url
 *
 * @return	true if the target is not above root, false otherwise
 */
bool	urlTargetAboveRoot(std::string_view url)
{
	std::vector<std::string>	parts	= splitUrl(url);
	size_t						up		= 0;
	size_t						down	= 0;

	for (auto const &p : parts) {
		if (p == "..") {
			++up;
		} else if (p == ".") {
			;
		} else if (!p.empty()) {
			++down;
		}
	}

	return (up > down);
}

#ifdef TEST

#include <iostream>
#include <cassert>

int	main()
{
	std::cout << getImfFixdate() << "\n";
	assert(isValidIPv4("1.1.1.1") == true);
	assert(isValidIPv4("0.0.0.0") == true);
	assert(isValidIPv4("255.255.255.255") == true);
	assert(isValidIPv4("0.0.256.0") == false);
	assert(isValidIPv4("-0.0.0.0") == false);
	assert(isValidIPv4("1.1.1") == false);
	assert(isValidIPv4("1.1.1.") == false);
	assert(isValidIPv4(".1.1.1") == false);
	assert(isValidIPv4("1.a.1.1") == false);
	assert(isValidIPv4("255.255.255.255.255") == false);

	auto	urlSplitTest = splitUrl("asdf//asdfsadfsadf");
	for (auto const &e : urlSplitTest)
		std::cout << "\"" << e << "\" ";
	std::cout << "\n";
	return 0;
}

#endif
