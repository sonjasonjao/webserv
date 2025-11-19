#pragma once

#include <exception>
#include <string>

class CustomeException : public std::exception {
    protected:
        std::string _error_message;

    public:
        CustomeException (void) noexcept;
        CustomeException (const std::string& msg) noexcept;
        CustomeException (const CustomeException& other) noexcept;
        CustomeException& operator=(const CustomeException& other) noexcept;
        ~CustomeException() noexcept;

        const char* what() const noexcept;
};
