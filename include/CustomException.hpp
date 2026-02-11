#pragma once

#include <exception>
#include <string>

class CustomException : public std::exception {

protected:
	std::string _errorMessage;

public:
	CustomException() noexcept;
	CustomException(std::string const &msg) noexcept;
	CustomException(CustomException const &other) noexcept;
	~CustomException() noexcept = default;

	CustomException	&operator=(CustomException const &other) noexcept;

	char const	*what() const noexcept;
};
