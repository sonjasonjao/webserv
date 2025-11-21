#include <gtest/gtest.h>
#include "../include/Parser.hpp"
#include "../include/CustomeException.hpp"

// 1) No argument constructor should throw an exception
TEST(ParserGetMessageTest, DefaultCtorThrows) {
    Parser p;

    EXPECT_THROW(p.getMessage(), CustomeException);
}

// 2) Custom string should be returned correctly and should not throw an exception
TEST(ParserGetMessageTest, CustomStringReturnsValue) {
    std::string expected = "Heloooooo";
    Parser p(expected);

    std::string actual = p.getMessage();

    EXPECT_EQ(actual, expected);
}

// 3) ParserException derives from CustomeException class
TEST(ParserGetMessageTest, DefaultExceptionMessage) {
    Parser p;

    try {
        p.getMessage();
        FAIL() << "Expected ParserException to be thrown";
    } catch (const CustomeException& e) {
        EXPECT_STREQ("Default error!", e.what());
    } catch (...) {
        FAIL() << "Expected ParserException, got a different exception type";
    }
}
