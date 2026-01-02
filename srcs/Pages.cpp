#include "Pages.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include <stdexcept>

std::unordered_map<std::string, std::string>			Pages::defaultPages;
std::unordered_map<std::string, std::string const *>	Pages::cacheMap;
std::deque<std::pair<std::string, std::string>>			Pages::cacheQue;
size_t													Pages::cacheSize = 0;

constexpr static char const * const	DEFAULT200	= \
R"(
<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<p>200: OK</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT204	= \
R"(
<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<p>204: no content</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT400	= \
R"(
<!DOCTYPE html>
<html>
	<head>
		<style>
			body, html {
				color: yellow;
				width: 100%;
				height: 100%;
				margin: 0;
				padding: 0;
			}
			p {
				font-size: 5rem;
				display: block;
				text-align: center;
				vertical-align: middle;
				margin: 0 auto;
				margin-top: 2rem;
				color: red;
			}
		</style>
	</head>
	<body>
		<p>400: bad request</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT404	= \
R"(
<!DOCTYPE html>
<html>
	<head>
		<style>
			body, html {
				color: yellow;
				width: 100%;
				height: 100%;
				margin: 0;
				padding: 0;
			}
			p {
				font-size: 5rem;
				display: block;
				text-align: center;
				vertical-align: middle;
				margin: 0 auto;
				margin-top: 2rem;
				color: red;
			}
		</style>
	</head>
	<body>
		<p>404: resource not found</p>
	</body>
</html>
)";

void	Pages::loadDefaults()
{
	defaultPages.clear();
	defaultPages["default200"] = DEFAULT200;
	defaultPages["default204"] = DEFAULT204;
	defaultPages["default400"] = DEFAULT400;
	defaultPages["default404"] = DEFAULT404;
}

bool	Pages::isCached(std::string const &key)
{
	try {
		cacheMap.at(key);
		return true;
	} catch (std::out_of_range const &e) {}
	try {
		defaultPages.at(key);
		return true;
	} catch (std::out_of_range const &e) {}
	return false;
}

/**
 * Looks for a given page in the cache and returns it. If the page can't be found in
 * the cache, attempts to add it to the cache and then returns it.
 *
 * NOTE:	Page validation has to have happened before this step, assumes
 *			an absolute path for non default pages
 *
 * @return	string containing page content
 */
std::string const	&Pages::getPageContent(std::string const &key)
{
	INFO_LOG("Retrieving " + key);

	try {
		return defaultPages.at(key);
	} catch (std::out_of_range const &e) {}
	try {
		return *cacheMap.at(key);
	} catch (std::out_of_range const &e) {}

	std::string	page = getFileAsString(key, "/");	// Force absolute filepath for unique identifiers for resources

	if (page.length() > CACHE_SIZE_MAX)
		throw std::runtime_error(ERROR_LOG("File " + key + " is too large for cache"));

	while (cacheSize > 0 && cacheSize > CACHE_SIZE_MAX - page.length()) {
		DEBUG_LOG("Removing " + cacheQue.front().first + " from cache to make space for " + key);
		cacheSize -= cacheQue.front().second.length();
		cacheMap.erase(cacheQue.front().first);
		cacheQue.pop_front();
	}

	DEBUG_LOG("Adding " + key + " to cache");
	cacheSize += page.length();
	cacheQue.emplace_back(key, page);
	cacheMap[key] = &cacheQue.back().second;

	return *cacheMap.at(key);
}

void	Pages::clearCache()
{
	cacheMap.clear();
	cacheQue.clear();
	cacheSize = 0;
}
