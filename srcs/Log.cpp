#include "Log.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <unistd.h>

/* ------------------------------------------------------------------- colors */

#define RED	"\033[0;31m\001"
#define CYA	"\033[0;36m\001"
#define YEL	"\033[0;93m\001"
#define CLR	"\033[0m\002"

/* ----------------------------------------------------------- implementation */

std::ofstream	Log::_ofs;	// Reserve space for static member variable

/**
 * Logs timestamps for info, debug, and error logging functions. Currently the
 * format is YYYY-MM-DD HH:MM:SS, adjustable padding via macro TIMESTAMP_W in
 * Log.hpp. Takes into account if a logfile is open, otherwise it is possible to
 * pass fds for stdout or stderr to select preferred output.
 * TODO: support other fds
 */
void	Log::logTime(int fd)
{
	auto	timePoint	= std::chrono::system_clock::now();
	auto	time		= std::chrono::system_clock::to_time_t(timePoint);

	std::stringstream	timeStream;
	std::string			timeStr;

	timeStream	<< std::put_time(std::localtime(&time), "%F %T");
	timeStr		= timeStream.str();

	if (_ofs.is_open()) {
		_ofs << std::setw(TIMESTAMP_W) << std::left << timeStr;
		return;
	}

	if (fd == 2) {
		std::cerr << std::setw(TIMESTAMP_W) << std::left << timeStr;
		return;
	}

	std::cout << std::setw(TIMESTAMP_W) << std::left << timeStr;
}

/**
 * Error logging
 */
void	Log::error(std::string_view file, std::string_view function,
				int line, std::string_view message)
{
	std::string	errorMarker = "ERROR: ";

	logTime(STDERR_FILENO);	// Always log a time stamp

	if (_ofs.is_open() || !isatty(0) || !isatty(1)) {
		if (errno == EBADF) {
			std::cerr << "ERROR: can't establish tty status\n";
		}
		_ofs	<< std::setw(CATEGORY_W) << std::right << errorMarker << file
				<< ": " << function << " (" << line << "): " << message << "\n";
		return;
	}

	errorMarker = RED + errorMarker + CLR;
	std::cerr	<< std::setw(CATEGORY_W) << std::right << errorMarker << file
				<< ": " << function << " (" << line << "): " << message << "\n";
}

/**
 * Debug logging
 */
void	Log::debug(std::string_view file, std::string_view function,
				int line, std::string_view message)
{
	std::string	debugMarker = "DEBUG: ";

	logTime(STDERR_FILENO);	// Always log a time stamp

	if (_ofs.is_open() || !isatty(0) || !isatty(1)) {
		if (errno == EBADF) {
			std::cerr << "ERROR: can't establish tty status\n";
		}
		_ofs	<< std::setw(CATEGORY_W) << std::right << debugMarker << file
				<< ": " << function << " (" << line << "): " << message << "\n";
		return;
	}

	debugMarker = YEL + debugMarker + CLR;
	std::cout	<< std::setw(CATEGORY_W) << std::right << debugMarker << file
				<< ": " << function << " (" << line << "): " << message << "\n";
}

/**
 * Info logging
 */
void	Log::info(std::string_view functionName, std::string_view context)
{
	std::string	infoMarker = "INFO: ";

	logTime(STDOUT_FILENO);

	if (_ofs.is_open()) {
		_ofs	<< std::setw(CATEGORY_W) << std::right << infoMarker
				<< functionName << ": " << context << "\n";
		return;
	}

	infoMarker = CYA + infoMarker + CLR;
	std::cout	<< std::setw(CATEGORY_W) << std::right << infoMarker
				<< functionName << ": " << context << "\n";
}

/**
 * Sets output file to be used by logging functions, using a file name string
 * view. Closes previous file if open, throws runtime_error in case a file
 * operation fails.
 */
void	Log::setOuputFile(std::string_view outputFileName)
{
	if (_ofs.is_open()) {
		_ofs.close();
		if (_ofs.failbit) {
			throw std::runtime_error("setOutputFile: couldn't close previously opened output file stream");
		}
	}
	_ofs.open(std::string(outputFileName));
	if (!_ofs.is_open()) {
		throw std::runtime_error("setOutputFile: couldn't open file");
	}
}
