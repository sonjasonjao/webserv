#include "Utils.hpp"
#include "Log.hpp"
#include <array>
#include <limits>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <clocale>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cerrno>
#include <cstring>

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

	timeStream.imbue(std::locale::classic());
	timeStream << std::put_time(std::gmtime(&nowTimeT), "%a, %d %b %Y %H:%M:%S GMT");

	return timeStream.str();
}

/**
 * @return	std::string with all characters that satisfy std::isspace removed
 *			from the front and back of the string view input
 */
std::string	trimWhitespace(std::string_view	sv)
{
	while (!sv.empty() && std::isspace(sv[0]))
		sv = sv.substr(1);
	while (!sv.empty() && std::isspace(sv[sv.length() - 1]))
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
	std::string_view			orig	= sv;
	size_t						pos		= 0;

	while (!sv.empty()) {
		pos = sv.find(delim);
		res.emplace_back(sv.substr(0, pos));
		if (pos == std::string::npos)
			break;
		sv = sv.substr(pos + delim.length());
	}

	if (orig.length() >= delim.length()
		&& orig.substr(orig.length() - delim.length()) == delim)
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
 * @return	true when sv contains port in valid range, false if not
 */
bool	isValidPort(std::string_view sv)
{
	if (sv.empty())
		return false;

	if (!std::all_of(sv.begin(), sv.end(), isdigit))
		return false;

	unsigned long	portNumber;

	try {
		portNumber = std::stoul(std::string(sv));
	} catch (std::exception const &e) {
		return false;
	}

	if (portNumber > std::numeric_limits<uint16_t>::max())
		return false;
	if (portNumber < 1024)
		return false;

	return true;
}

/**
 * @return	true if resource can be found in the given directory, false if not
 */
bool	resourceExists(std::string_view uri, std::string searchDir)
{
	static std::string const	currentDir = std::filesystem::current_path();

	if (uri.empty())
		return false;

	if (searchDir.empty())
		searchDir = currentDir;

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
 * Check whether a given URI tries to access files beyond root
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

	return up > down;
}

/**
 * @return	true if sv contains a valid IMF fixdate, false if not
 */
bool	isValidImfFixdate(std::string_view sv)
{
	if (sv.empty())
		return false;

	auto	parts = splitStringView(sv);

	if (parts.empty())
		return false;
	if (parts.size() != 6)
		return false;
	if (std::any_of(parts.begin(), parts.end(), [](auto a) {return a.empty();}))
		return false;

	static const std::array<std::string, 7>	weekdays = {
		"Mon,",
		"Tue,",
		"Wed,",
		"Thu,",
		"Fri,",
		"Sat,",
		"Sun,"
	};

	if (std::none_of(weekdays.begin(), weekdays.end(), [&parts](auto a) {return parts[0] == a;}))
		return false;

	if (parts[1].length() > 2)
		return false;
	if (!std::all_of(parts[1].begin(), parts[1].end(), isdigit))
		return false;

	int	day = std::stoi(parts[1]);

	if (day < 1 || day > 31)
		return false;

	static const std::array<std::string, 12>	months = {
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec",
	};

	int	month = -1;

	for (int i = 0; i < 12; ++i)
		if (months[i] == parts[2])
			month = i;

	if (month < 0)
		return false;

	if (parts[3].length() != 4)
		return false;
	if (!std::all_of(parts[3].begin(), parts[3].end(), isdigit))
		return false;

	int	year = std::stoi(parts[3]);

	std::chrono::year_month_day	ymd(	static_cast<std::chrono::year>(year),
										static_cast<std::chrono::month>(month + 1),
										static_cast<std::chrono::day>(day));

	if (!ymd.ok())
		return false;

	auto	hms = splitStringView(parts[4], ":");	// hours:minutes:seconds

	if (hms.size() != 3)
		return false;
	if (std::any_of(hms.begin(), hms.end(), [](auto a) {return a.empty();}))
		return false;
	for (auto const &e : hms) {
		if (e.length() != 2 || !std::all_of(e.begin(), e.end(), isdigit))
			return false;
	}

	int	hours	= std::stoi(hms[0]);
	int	minutes	= std::stoi(hms[1]);
	int	seconds	= std::stoi(hms[2]);

	if (hours > 23 || minutes > 59 || seconds > 59)
		return false;

	if (parts[5] != "GMT")
		return false;

	return true;
}

/**
 * Loads a complete file into a std::string, no removal of whitespace or other
 * special characters.
 *
 * NOTE: Throws a runtime error if file can't be opened, assumes prior validation
 *
 * @return	String object containing fileName's contents
 */
std::string	getFileAsString(std::string const &fileName, std::string searchDir)
{
	static std::string const	currentDir = std::filesystem::current_path();

	if (searchDir.empty())
		searchDir = currentDir;
	if (searchDir.back() == '/')
		searchDir.pop_back();

	std::ifstream		fileStream(searchDir + "/" + fileName);

	if (fileStream.fail())
		throw std::runtime_error(ERROR_LOG("Couldn't open " + fileName + ": " + strerror(errno)));

	std::stringstream	buf;

	buf << fileStream.rdbuf();

	return buf.str();
}

/**
 * NOTE:	Assumes that we won't be changing the current path while running our
 *			program, otherwise currentDir won't be valid anymore.
 *
 * @return	String representation of the absolute path of fileName
 */
std::string	getAbsPath(std::string const &fileName, std::string searchDir)
{
	static std::string const	currentDir = std::filesystem::current_path();

	if (!fileName.empty() && fileName[0] == '/')
		return fileName;

	if (searchDir.empty())
		searchDir = currentDir;
	if (searchDir.back() == '/')
		searchDir.pop_back();
	if (fileName.front() == '/')
		return searchDir + fileName;

	return searchDir + "/" + fileName;
}

bool	isUnsignedIntLiteral(std::string_view sv)
{
	if (sv.empty())
		return false;

	auto	i = sv.begin();

	if (*i == '+')
		++i;
	if (i == sv.end() || !isdigit(*(i++)))
		return false;

	if (!std::all_of(i, sv.end(), isdigit))
		return false;

	try {
		if (std::stoul(std::string(sv)) > std::numeric_limits<unsigned int>::max())
			return false;
	} catch (std::exception const &e) {
		return false;
	}

	return true;
}

bool	isPositiveDoubleLiteral(std::string_view sv)
{
	bool	hasWholePart		= false;
	bool	hasFractionalPart	= false;

	if (sv.empty()) {
		return false;
	}

	auto	i = sv.begin();

	if (*i == '+')
		++i;

	if (std::isdigit(*i))
		hasWholePart = true;
	while (i != sv.end() && isdigit(*i))
		++i;
	if (i == sv.end() || *i != '.')
		return false;
	++i;

	if (i != sv.end() && isdigit(*i))
		hasFractionalPart = true;
	while (i != sv.end() && isdigit(*i))
		++i;

	if (i != sv.end() || (!hasWholePart && !hasFractionalPart))
		return false;

	try {
		std::stod(std::string(sv));
	} catch ( std::exception const &e ) {
		return false;
	}

	return true;
}

/**
 * helper function to extract a value from a string based on a prefix
*/
std::string	extractValue(const std::string& source, const std::string& key)
{
	size_t	pos = source.find(key);

	if (pos == std::string::npos) {
		return "";
	}
	pos += key.length();

	size_t	end = source.find_first_of("\r\n,;", pos);

	return source.substr(pos, end - pos);
}

/**
 * helper function to extract a quoted value from a string based on a prefix
*/
std::string	extractQuotedValue(const std::string& source, const std::string& key)
{
	size_t	pos = source.find(key);

	if (pos == std::string::npos) {
		return "";
	}

	pos += key.length();

	size_t	quote_start = source.find('"', pos);

	if (quote_start == std::string::npos) {
		return "";
	}

	size_t	quote_end = source.find('"', quote_start + 1);

	if (quote_end == std::string::npos) {
		return "";
	}

	return source.substr(quote_start + 1, quote_end - quote_start - 1);
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

	assert(isValidImfFixdate("Tue, 15 Nov 1994 12:45:26 GMT") == true);
	assert(isValidImfFixdate("tue, 15 Nov 1994 12:45:26 GMT") == false);
	assert(isValidImfFixdate("Tue, 32 Nov 1994 12:45:26 GMT") == false);
	assert(isValidImfFixdate("Tue, 15 Nov 1994 12:45:60 GMT") == false);
	assert(isValidImfFixdate("Tue, 15 Nov 1994 12:45:26") == false);
	return 0;
}

#endif
