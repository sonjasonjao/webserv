#include <cerrno>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <string_view>
#include <unistd.h>

#define RED	"\033[0;31m\001"
#define CLR	"\033[0m\002"

#define ERROR

#ifdef ERROR
# define ERROR_LOG(MESSAGE)	Log::error(__FILE__, __FUNCTION__, __LINE__, MESSAGE)
#else
# define ERROR_LOG(MESSAGE)
#endif

class Log {

public:
	Log() = delete;
	Log(std::string const &outputFileName) = delete;
	Log &operator=(Log const &other) = delete;
	~Log() = default;

	static void	error(std::string_view file, std::string_view function,
				   int line, std::string_view message);
	static void	info(std::string_view functionName);
	static void	debug(std::string_view function_name);
	static void	setOuputFile(std::string_view outputFileName);

private:
	static void				logTime(int fd);
	static std::ofstream	_ofs;
};

std::ofstream	Log::_ofs;

void	Log::logTime(int fd)
{
	auto	timePoint	= std::chrono::system_clock::now();
	auto	time		= std::chrono::system_clock::to_time_t(timePoint);

	if (_ofs.is_open()) {
		_ofs << std::put_time(std::localtime(&time), "%F %T");
		_ofs << "\t";
		return;
	}

	if (fd == 2) {
		std::cerr << std::put_time(std::localtime(&time), "%F %T");
		std::cerr << "\t";
		return;
	}

	std::cout << std::put_time(std::localtime(&time), "%F %T");
	std::cout << "\t";
}

void	Log::error(std::string_view file, std::string_view function, 
				int line, std::string_view message)
{
	std::string	errorMarker;

	logTime(2);	// Always log a time stamp

	if (_ofs.is_open() || !isatty(0) || !isatty(1)) {
		if (errno == EBADF) {
			std::cerr << "ERROR: can't establish tty status\n";
		}
		errorMarker = "ERROR: ";
	} else {
		errorMarker = RED "ERROR: " CLR;
	}

	if (_ofs.is_open()) {
		_ofs	<< errorMarker << file << ": " << function
				<< ": line " << line << ": " << message << "\n";
		return;
	}

	std::cerr	<< errorMarker << file << ": " << function
				<< ": line " << line << ": " << message << "\n";
}

void	Log::setOuputFile(std::string_view outputFileName)
{
	if (_ofs.is_open()) {
		_ofs.close();
		if (_ofs.failbit) {
			throw std::runtime_error("setOutputFile: couldn't close previously opened file");
		}
	}
	_ofs.open(std::string(outputFileName));
	if (!_ofs.is_open()) {
		throw std::runtime_error("setOutputFile: couldn't open file");
	}
}

void	endianTest()
{
	uint32_t	num		= 0xa1b2c3d4;
	uint8_t		*ptr	= (uint8_t *)&num;

	printf("First byte: %x\n", *ptr);

	if (*ptr == 0xa1)
		std::cout << "Big endian\n";
	else
		std::cout << "Little endian\n";
}

bool	isBigEndian()
{
	uint32_t	num		= 0xa1b2c3d4;
	uint8_t		*ptr	= (uint8_t *)&num;

	return (*ptr == (num >> 24));
}

bool	isLittleEndian()
{
	uint32_t	num		= 0xa1b2c3d4;
	uint8_t		*ptr	= (uint8_t *)&num;

	return (*ptr == (num & 0xff));
}

int	main()
{
	if (isLittleEndian())
		endianTest();
	ERROR_LOG("I need a break");
	return 0;
}
