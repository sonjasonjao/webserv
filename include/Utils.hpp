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
// Validates that the given string represents a non-privileged TCP/UDP port.
// Ports below 1024 are intentionally rejected because they are typically
// reserved as privileged ports on Unix-like systems.
bool	                    isValidPort(std::string_view sv);
bool						isValidImfFixdate(std::string_view sv);

std::string					getFileAsString(std::string const &fileName, std::string searchDir = "");
std::string					getAbsPath(std::string const &fileName, std::string searchDir = "");
