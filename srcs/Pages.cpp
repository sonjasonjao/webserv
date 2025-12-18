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
	cacheQue.emplace_back("default200", DEFAULT200);
	cacheMap["default200"] = &cacheQue.back().second;
	cacheQue.emplace_back("default204", DEFAULT204);
	cacheMap["default204"] = &cacheQue.back().second;
	cacheQue.emplace_back("default400", DEFAULT400);
	cacheMap["default400"] = &cacheQue.back().second;
	cacheQue.emplace_back("default404", DEFAULT404);
	cacheMap["default404"] = &cacheQue.back().second;
	cacheSize += cacheMap["default200"]->length();
	cacheSize += cacheMap["default204"]->length();
	cacheSize += cacheMap["default400"]->length();
	cacheSize += cacheMap["default404"]->length();
}

bool	Pages::isCached(std::string const &key)
{
	try {
		cacheMap.at(key);
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
	INFO_LOG("Retrieving " + key);
	try {
		std::string const	&content = *cacheMap.at(key);
		
		return content;
	} catch (std::out_of_range const &e) {}

	std::string	page = getFileAsString(key, "/");	// Force absolute filepath for unique identifiers for resources

	if (page.length() > CACHE_SIZE_MAX)
		throw std::runtime_error(ERROR_LOG("File " + key + " is too large for cache"));

	while (cacheSize > 0 && cacheSize + page.length() > CACHE_SIZE_MAX) {
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
