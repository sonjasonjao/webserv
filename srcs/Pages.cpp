#include "Pages.hpp"
#include "Utils.hpp"
#include "Log.hpp"

std::unordered_map<std::string, std::string>			Pages::defaultPages;
std::list<std::pair<std::string, std::string>>			Pages::cacheQueue;
std::unordered_map<std::string, std::string const *>	Pages::cacheMap;
std::string												Pages::bigFileName;
std::string												Pages::bigFile;
size_t													Pages::cacheSize = 0;

constexpr static char const * const	DEFAULT200	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>200 OK</title>
	</head>
	<body>
		<h1>200: OK</h1>
		<p>Your request was processed successfully.</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT204	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>204 No Content</title>
	</head>
	<body>
		<h1>204: No Content</h1>
		<p>Your request was processed successfully.</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT201	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>201 Created</title>
	</head>
	<body>
		<h1>201: Created</h1>
		<p>Your request was processed successfully.</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT400	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>400 Bad Request</title>
	</head>
	<body>
		<h1>400: Bad Request</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT403	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>403 Forbidden</title>
	</head>
	<body>
		<h1>403: Forbidden</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT404	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>404 Not Found</title>
	</head>
	<body>
		<h1>404: Resource Not Found</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT405	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>405 Method Not Allowed</title>
	</head>
	<body>
		<h1>405: Method Not Allowed</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT408	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>408 Request Timeout</title>
	</head>
	<body>
		<h1>408: Request Timeout</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT409	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>409 Conflict</title>
	</head>
	<body>
		<h1>409: Conflict</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT413	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>413 Content Too Large</title>
	</head>
	<body>
		<h1>413: Content Too Large</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT500	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>500 Internal Server Error</title>
	</head>
	<body>
		<h1>500: Internal Server Error</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

constexpr static char const * const	DEFAULT504	= \
R"(<!DOCTYPE html>
<html>
	<head>
		<title>504 Gateway Timeout</title>
	</head>
	<body>
		<h1>504: Gateway Timeout</h1>
		<p>Oh no!</p>
	</body>
</html>
)";

void	Pages::loadDefaults()
{
	defaultPages.clear();
	defaultPages["default200"] = DEFAULT200;
	defaultPages["default204"] = DEFAULT204;
	defaultPages["default201"] = DEFAULT201;
	defaultPages["default400"] = DEFAULT400;
	defaultPages["default403"] = DEFAULT403;
	defaultPages["default404"] = DEFAULT404;
	defaultPages["default405"] = DEFAULT405;
	defaultPages["default408"] = DEFAULT408;
	defaultPages["default409"] = DEFAULT409;
	defaultPages["default413"] = DEFAULT413;
	defaultPages["default500"] = DEFAULT500;
	defaultPages["default504"] = DEFAULT504;
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
 * @return	String containing page content
 */
std::string const	&Pages::getPageContent(std::string const &key)
{
	DEBUG_LOG("Retrieving " + key);

	if (defaultPages.find(key) != defaultPages.end())
		return defaultPages.at(key);
	if (cacheMap.find(key) != cacheMap.end())
		return *cacheMap.at(key);
	if (key == bigFileName)
		return bigFile;

	std::string	page = getFileAsString(key, "/"); // Force absolute filepath for unique identifiers for resources

	if (page.length() > CACHE_SIZE_MAX) {
		DEBUG_LOG("File '" + key + "' is too large for cache");
		bigFile = page;
		bigFileName = key;
		return bigFile;
	}

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
