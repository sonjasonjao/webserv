#include "Log.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

/* ------------------------------------------------------------------- colors */

#define RED	"\033[0;31m"
#define CYA	"\033[0;36m"
#define YEL	"\033[0;93m"
#define CLR	"\033[0m"

/* ----------------------------------------------------------- implementation */

std::ofstream	Log::_ofs;	// Reserve space for static member variable

/**
 * Logs timestamps for info, debug, and error logging functions. Currently the
 * format is YYYY-MM-DD HH:MM:SS, adjustable padding via macro TIMESTAMP_WIDTH in
 * Log.hpp. Takes into account if a logfile is open, otherwise checks log level
 * for correct output stream.
 */
void	Log::logTime(std::ostream *outputStream)
{
	auto	timePoint	= std::chrono::system_clock::now();
	auto	time		= std::chrono::system_clock::to_time_t(timePoint);

	std::stringstream	timeStream;
	std::string			timeStr;
	std::ostream		*outputStream = &std::cout;

	timeStream	<< std::put_time(std::localtime(&time), "%F %T");
	timeStr		= timeStream.str();

	*outputStream << std::setw(TIMESTAMP_WIDTH) << std::left << timeStr;
}

/**
 * Helper function to unify log functionality. Checks message type and if
 * parameters are valid. file, function and line have default values in header.
 */
std::string	Log::logMessage(LogType type, std::string_view message,
					std::string_view file, std::string_view function, int line)
{
	std::string			typeString;
	std::string			color;
	std::string			returnString;
	std::stringstream	strStream;
	std::ostream		*outputStream = &std::cout;

	switch (type) {
		case LogType::Info:
			typeString = "INFO: ";
			color = CYA;
			break;
		case LogType::Debug:
			typeString = "DEBUG: ";
			color = YEL;
			break;
		case LogType::Error:
			typeString = "ERROR: ";
			color = RED;
			break;
		default:
			std::string	msg = "logMessage: unknown type";
			throw std::runtime_error(msg);
	};

	if (type != LogType::Info)
		outputStream = &std::cerr;

	if (_ofs.is_open())
		outputStream = &_ofs;

	Log::logTime(outputStream);

	if (!_ofs.is_open() && ((type == LogType::Info && isatty(STDOUT_FILENO))
		|| (type != LogType::Info && isatty(STDERR_FILENO))))
		typeString = color + typeString + CLR;

	strStream << std::setw(CATEGORY_WIDTH) << std::right << typeString;
	if (!file.empty())
		strStream << file << ": ";
	if (!function.empty())
		strStream << function << ": ";
	if (line > 0)
		strStream << line << ": ";
	strStream << message << "\n";

	returnString = strStream.str();
	*outputStream << returnString;

	return returnString;
}

/**
 * Error logging
 */
std::string	Log::error(std::string_view file, std::string_view function,
				int line, std::string_view message)
{
	return logMessage(LogType::Error, message, file, function, line);
}

/**
 * Debug logging
 */
std::string	Log::debug(std::string_view file, std::string_view function,
				int line, std::string_view message)
{
	return logMessage(LogType::Debug, message, file, function, line);
}

/**
 * Info logging
 */
std::string	Log::info(std::string_view message)
{
	return logMessage(LogType::Info, message);
}

/**
 * Sets output file to be used by logging functions, using a file name string
 * view. Closes previous file if open, throws runtime_error in case a file
 * operation fails.
 */
void	Log::setOutputFile(std::string_view outputFileName)
{
	if (_ofs.is_open()) {
		_ofs.close();
		if (_ofs.fail()) {
			std::string	msg = "setOutputFile: couldn't close previously opened output file stream";
			throw std::runtime_error(msg);
		}
	}
	_ofs.open(std::string(outputFileName));
	if (!_ofs.is_open()) {
		std::string	msg = "setOutputFile: couldn't open " + std::string(outputFileName);
		throw std::runtime_error(msg);
	}
}
