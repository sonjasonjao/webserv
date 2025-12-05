#include "Pages.hpp"

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
	cache["default200"] = DEFAULT200;
	cache["default400"] = DEFAULT400;
	cache["default404"] = DEFAULT404;
	cacheSize += cache["default200"].length();
	cacheSize += cache["default400"].length();
	cacheSize += cache["default404"].length();
}
