#pragma once

#include <exception>
#include <string>

class CustomException : public std::exception {
protected:
	std::string _error_message;

public:
	CustomException() noexcept;
	CustomException(std::string const &msg) noexcept;
	CustomException(CustomException const &other) noexcept;
	~CustomException() noexcept;

	CustomException	&operator=(CustomException const &other) noexcept;

	const char	*what() const noexcept;
};
