#pragma once

#include <string>
#include <cstddef>
#include <unordered_map>
#include <deque>

#define CACHE_SIZE_MAX	4194304	// 4 MiB

class Pages {

public:
	static bool					isCached(std::string const &key);
	static std::string const	&getPageContent(std::string const &key);
	static void					clearCache();
	static void					loadDefaults();

private:
	static std::unordered_map<std::string, std::string>			defaultPages;
	static std::deque<std::pair<std::string, std::string>>		cacheQue;
	static std::unordered_map<std::string, std::string const *>	cacheMap;
	static size_t												cacheSize;
};
