#include "CustomeException.hpp"

CustomeException::CustomeException(void) noexcept:
_error_message("unknown"){}

CustomeException::CustomeException (const std::string& msg) noexcept:
_error_message(msg){}

CustomeException::CustomeException (const CustomeException& other) noexcept:
_error_message(other._error_message) {}

CustomeException& CustomeException::operator=(const CustomeException& other) noexcept {
    if(this != &other) {
        this->_error_message = other._error_message;
    }
    return (*this);
}

CustomeException::~CustomeException() noexcept {}

const char* CustomeException::what() const noexcept {
    return (_error_message.c_str());
}
