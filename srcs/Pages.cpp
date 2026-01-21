#include "Pages.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include <stdexcept>

std::unordered_map<std::string, std::string>			Pages::defaultPages;
std::list<std::pair<std::string, std::string>>			Pages::cacheQueue;
std::unordered_map<std::string, std::string const *>	Pages::cacheMap;
size_t													Pages::cacheSize = 0;

constexpr static char const * const	DEFAULT200	= \
R"(<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<h1>200: OK</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT204	= \
R"(<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<h1>204: No Content</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT400	= \
R"(<!DOCTYPE html>
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
		<h1>400: Bad Request</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT403	= \
R"(<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<h1>403: Forbidden</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT404	= \
R"(<!DOCTYPE html>
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
		<h1>404: Resource Not Found</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT408	= \
R"(<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<h1>408: Request Timeout</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT413	= \
R"(<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<h1>413: Content Too Large</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT500	= \
R"(<!DOCTYPE html>
<html>
	<head>
	</head>
	<body>
		<h1>500: Internal Server Error</h1>
		<p>Default fallback page</p>
	</body>
</html>
)";

void	Pages::loadDefaults()
{
	defaultPages.clear();
	defaultPages["default200"] = DEFAULT200;
	defaultPages["default204"] = DEFAULT204;
	defaultPages["default400"] = DEFAULT400;
	defaultPages["default403"] = DEFAULT403;
	defaultPages["default404"] = DEFAULT404;
	defaultPages["default408"] = DEFAULT408;
	defaultPages["default413"] = DEFAULT413;
	defaultPages["default500"] = DEFAULT500;
}

bool	Pages::isCached(std::string const &key)
{
	if (cacheMap.find(key) != cacheMap.end())
		return true;
	if (defaultPages.find(key) != defaultPages.end())
		return true;
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

	if (defaultPages.find(key) != defaultPages.end())
		return defaultPages.at(key);
	if (cacheMap.find(key) != cacheMap.end())
		return *cacheMap.at(key);

	std::string	page = getFileAsString(key, "/");	// Force absolute filepath for unique identifiers for resources

	if (page.length() > CACHE_SIZE_MAX)
		throw std::runtime_error(ERROR_LOG("File '" + key + "' is too large for cache"));

	while (cacheSize > 0 && cacheSize > CACHE_SIZE_MAX - page.length()) {
		DEBUG_LOG("Removing " + cacheQueue.front().first + " from cache to make space for " + key);
		cacheSize -= cacheQueue.front().second.length();
		cacheMap.erase(cacheQueue.front().first);
		cacheQueue.pop_front();
	}

	DEBUG_LOG("Adding " + key + " to cache");
	cacheQueue.emplace_back(key, page);
	cacheMap[key] = &cacheQueue.back().second;
	cacheSize += page.length();

	return *cacheMap.at(key);
}

void	Pages::clearCache()
{
	cacheMap.clear();
	cacheQueue.clear();
	cacheSize = 0;
}
