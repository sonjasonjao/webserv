/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Pages.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jvarila <jvarila@student.hive.fi>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/09 11:33:23 by jvarila           #+#    #+#             */
/*   Updated: 2025/12/09 13:59:36 by jvarila          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Pages.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include <stdexcept>

std::unordered_map<std::string, std::string>	Pages::cache;
size_t											Pages::cacheSize = 0;

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
	</head>
	<body>
		<p>404: resource not found</p>
	</body>
</html>
)";

void	Pages::loadDefaults()
{
	try {
		cacheSize -= cache.at("default200").length();
		cache.erase("default200");
	} catch (std::out_of_range const &e) {}
	try {
		cacheSize -= cache.at("default400").length();
		cache.erase("default400");
	} catch (std::out_of_range const &e) {}
	try {
		cacheSize -= cache.at("default404").length();
		cache.erase("default404");
	} catch (std::out_of_range const &e) {}
	cache["default200"] = DEFAULT200;
	cache["default204"] = DEFAULT204;
	cache["default400"] = DEFAULT400;
	cache["default404"] = DEFAULT404;
	cacheSize += cache["default200"].length();
	cacheSize += cache["default204"].length();
	cacheSize += cache["default400"].length();
	cacheSize += cache["default404"].length();
}

bool	Pages::isCached(std::string const &key)
{
	try {
		cache.at(key);
		return true;
	} catch (std::out_of_range const &e) {
		return false;
	}
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
	INFO_LOG("Retrieving " + key + " as page content");
	try {
		return cache.at(key);
	} catch (std::out_of_range const &e) {}

	std::string	page = getFileAsString(key, "/");	// Force absolute filepath for unique identifiers for resources

	if (page.length() > CACHE_SIZE_MAX)
		throw std::runtime_error(ERROR_LOG("File " + key + " is too large for cache"));

	while (cacheSize > 0 && cacheSize + page.length() > CACHE_SIZE_MAX) {
		auto	it = cache.begin();

		DEBUG_LOG("Removing " + it->first + " from cache to make space for " + key);
		cacheSize -= it->second.length();
		cache.erase(it->first);
	}
	DEBUG_LOG("Adding " + key + " to cache");
	cache[key] = page;
	cacheSize += page.length();

	return cache.at(key);
}

void	Pages::clearCache()
{
	cache.clear();
	cacheSize = 0;
}
