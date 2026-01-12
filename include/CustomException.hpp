#pragma once

#include <exception>
#include <string>

class CustomException : public std::exception {
protected:
	std::string _error_message;

public:
	CustomException (void) noexcept;
	CustomException (const std::string& msg) noexcept;
	CustomException (const CustomException& other) noexcept;
	CustomException& operator=(const CustomException& other) noexcept;
	~CustomException() noexcept;

	const char* what() const noexcept;
};
