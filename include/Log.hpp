#pragma once

#include <fstream>
#include <string_view>

// Activators for different message types, set to 0 for deactivation
#define ERROR_LOGGING	1
#define INFO_LOGGING	1
#define DEBUG_LOGGING	1

#define TIMESTAMP_WIDTH	23
#define CATEGORY_WIDTH	20

enum logType : int {
	INFO,
	DEBUG,
	ERROR,
};

class Log {

public:
	Log() = delete;
	Log(Log const &other) = delete;
	Log &operator=(Log const &other) = delete;
	~Log() = delete;

	static void	error(std::string_view file, std::string_view function,
				   int line, std::string_view message);
	static void	info(std::string_view functionName, std::string_view message);
	static void	debug(std::string_view file, std::string_view function,
				   int line, std::string_view message);
	static void	setOutputFile(std::string_view outputFileName);

private:
	static void	logTime(std::ostream *outputStream);
	static void	logMessage(logType type, std::string_view message,
					std::string_view file = "",
					std::string_view function = "",
					int line = 0);

	static std::ofstream	_ofs;
};

/* ---------------------------------------------------------- macro interface */

#if ERROR_LOGGING
# define ERROR_LOG(MESSAGE)	Log::error(__FILE__, __FUNCTION__, __LINE__, (MESSAGE))
#else
# define ERROR_LOG(MESSAGE)
#endif

#if DEBUG_LOGGING
# define DEBUG_LOG(MESSAGE)	Log::debug(__FILE__, __FUNCTION__, __LINE__, (MESSAGE))
#else
# define DEBUG_LOG(MESSAGE)
#endif

#if INFO_LOGGING
# define INFO_LOG(MESSAGE)	Log::info(__FUNCTION__, (MESSAGE))
#else
# define INFO_LOG(MESSAGE)
#endif

/* -------------------------------------------------------------------------- */
