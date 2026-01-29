#include "CustomException.hpp"

CustomException::CustomException() noexcept:
	_error_message("unknown") {}

CustomException::CustomException(const std::string& msg) noexcept:
	_error_message(msg) {}

CustomException::CustomException(const CustomException& other) noexcept:
	_error_message(other._error_message) {}

CustomException&	CustomException::operator=(const CustomException& other) noexcept {
	if (this != &other) {
		this->_error_message = other._error_message;
	}
	return *this;
}

CustomException::~CustomException() noexcept {}

const char*	CustomException::what() const noexcept {
	return _error_message.c_str();
}
