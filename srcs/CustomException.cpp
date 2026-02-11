#include "CustomException.hpp"

CustomException::CustomException() noexcept
	: _errorMessage("unknown") {}

CustomException::CustomException(std::string const &msg) noexcept
	: _errorMessage(msg) {}

CustomException::CustomException(CustomException const &other) noexcept
	: _errorMessage(other._errorMessage) {}

CustomException	&CustomException::operator=(CustomException const &other) noexcept
{
	if (this != &other) {
		_errorMessage = other._errorMessage;
	}
	return *this;
}

char const	*CustomException::what() const noexcept
{
	return _errorMessage.c_str();
}
