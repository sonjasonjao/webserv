#include "CustomException.hpp"

CustomException::CustomException() noexcept:
	_error_message("unknown") {}

CustomException::CustomException(std::string const &msg) noexcept:
	_error_message(msg) {}

CustomException::CustomException(CustomException const &other) noexcept:
	_error_message(other._error_message) {}

CustomException&	CustomException::operator=(CustomException const &other) noexcept {
	if (this != &other) {
		this->_error_message = other._error_message;
	}
	return *this;
}

CustomException::~CustomException() noexcept {}

char const	*CustomException::what() const noexcept {
	return _error_message.c_str();
}
