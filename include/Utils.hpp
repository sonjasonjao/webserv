#pragma once

#include <string>
#include <string_view>
#include <vector>

std::string					getImfFixdate();
std::string					trimWhitespace(std::string_view	sv);
std::vector<std::string>	splitStringView(std::string_view sv, std::string_view delim = " ");
std::vector<std::string>	splitUri(std::string_view uri);
bool						isValidIPv4(std::string_view sv);
bool						resourceExists(std::string_view uri, std::string searchDir = "");
bool						uriFormatOk(std::string_view uri);
bool						uriTargetAboveRoot(std::string_view uri);
bool						isValidPort(std::string_view sv);
bool						isValidImfFixdate(std::string_view sv);
bool						isIntLiteral(std::string_view sv);
bool						isDoubleLiteral(std::string_view sv);

std::string					getFileAsString(std::string const &fileName, std::string searchDir = "");
std::string					getAbsPath(std::string const &fileName, std::string searchDir = "");
