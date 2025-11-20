#pragma

#include <fstream>

#define ERROR
#define INFO
#define DEBUG

#define TIMESTAMP_W	23	// These are slightly finicky at the moment
#define CATEGORY_W	20

class Log {

public:
	Log() = delete;
	Log(std::string const &outputFileName) = delete;
	Log &operator=(Log const &other) = delete;
	~Log() = default;

	static void	error(std::string_view file, std::string_view function,
				   int line, std::string_view message);
	static void	info(std::string_view functionName, std::string_view context);
	static void	debug(std::string_view file, std::string_view function,
				   int line, std::string_view function_name);
	static void	setOuputFile(std::string_view outputFileName);

private:
	static void				logTime(int fd);
	static std::ofstream	_ofs;
};

/* ---------------------------------------------------------- macro interface */

#ifdef ERROR
# define ERROR_LOG(MESSAGE)	Log::error(__FILE__, __FUNCTION__, __LINE__, MESSAGE)
#else
# define ERROR_LOG(MESSAGE)
#endif

#ifdef DEBUG
# define DEBUG_LOG(MESSAGE)	Log::debug(__FILE__, __FUNCTION__, __LINE__, MESSAGE)
#else
# define DEBUG_LOG(MESSAGE)
#endif

#ifdef INFO
# define INFO_LOG(MESSAGE)	Log::info(__FUNCTION__, MESSAGE)
#else
# define INFO_LOG(MESSAGE)
#endif

/* -------------------------------------------------------------------------- */
