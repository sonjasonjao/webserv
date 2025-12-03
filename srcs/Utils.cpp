#include "Utils.hpp"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <clocale>
#include <filesystem>
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
 * @return	std::string with all characters that satisfy std::isspace removed
 *			from the front and back of the string view input
 */
std::string	trimWhitespace(std::string_view	sv)
{
	while (std::isspace(sv[0]))
		sv = sv.substr(1);
	while (std::isspace(sv[sv.length() - 1]))
		sv = sv.substr(0, sv.length() - 1);

	return std::string(sv);
}

/**
 * Splitting function for URI and IP validation, doesn't perform trimming.
 * Default delimiter is a single space.
 *
 * NOTE:	Creates empty members for cases where two delimiters are next to each other,
 *			or when starting or ending with a delimiter
 *
 * @return	string vector containing the parts separated by delim
 */
std::vector<std::string>	splitStringView(std::string_view sv, std::string_view delim)
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

	if (sv.length() >= delim.length()
		&& sv.substr(sv.length() - delim.length()) == delim)
			res.push_back("");

	return res;
}

/**
 * @return	string vector containing the URI parts split by '/'
 */
std::vector<std::string>	splitUri(std::string_view uri)
{
	return splitStringView(uri, "/");
}

/**
 * Function checks if the string_view parameter contains a valid IPv4 address.
 *
 * NOTE: Assuming that the input string is trimmed in advance
 *
 * @return	true if IPv4 in sv is valid, false if not
 */
bool	isValidIPv4(std::string_view sv)
{
	if (sv.empty())
		return false;

	std::vector<std::string>	octets = splitStringView(sv, ".");

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
 * @return	true if resource can be found in the given directory, false if not
 */
bool	resourceExists(std::string_view uri, std::string searchDir)
{
	if (uri.empty())
		return false;

	if (searchDir.empty())
		searchDir = std::filesystem::current_path();

	std::string	path{uri};

	if (path[0] == '/')
		path = path.substr(1);
	if (searchDir.back() == '/')
		searchDir.pop_back();

	path = searchDir + "/" + path;

	return std::filesystem::exists(path);
}

/**
 * Validation is still very minimal, empty URIs and empty fields in URIs
 * currently invalidate the format, no invalid characters yet.
 *
 * @return	true if there are no empty fields, false if there are
 */
bool	uriFormatOk(std::string_view uri)
{
	auto	split = splitStringView(uri, "/");

	if (split.empty())
		return false;
	if (split[0].empty())
		split.erase(split.begin());
	if (split[split.size() - 1].empty())
		split.pop_back();

	for (auto const &s : split) {
		if (s.empty())
			return false;	// NOTE: add validation for invalid characters
	}

	return true;
}

/**
 * Check whether a give URI tries to access files beyond root
 *
 * NOTE: Assumes valid URI format
 *
 * @return	true if the target is not above root, false otherwise
 */
bool	uriTargetAboveRoot(std::string_view uri)
{
	std::vector<std::string>	parts	= splitUri(uri);
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

//#define TEST
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

	auto	uriSplitTest = splitUri("asdf//asdfsadfsadf");
	for (auto const &e : uriSplitTest)
		std::cout << "\"" << e << "\" ";
	std::cout << "\n";

	assert(uriFormatOk("/abc/") == true);
	assert(uriFormatOk("/") == true);
	assert(uriFormatOk("abc/") == true);
	assert(uriFormatOk("abc") == true);
	assert(uriFormatOk("") == false);
	assert(uriFormatOk("//") == false);
	return 0;
}

#endif
