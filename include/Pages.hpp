#pragma once

#include <string>
#include <unordered_map>

#define CACHE_SIZE_MAX	4194304	// 4 MiB

class Pages {

public:
	static std::string const	&getPageContent(std::string const &key);
	static void					clearCache();
	static void					loadDefaults();		// NOTE: Would be nice if this function would know the config parser's context

private:
	static std::unordered_map<std::string, std::string>	cache;
	static size_t										cacheSize;
};
