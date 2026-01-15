#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <sstream>
#include <array>
#include <chrono>
#include <iomanip>
#include <clocale>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cerrno>
#include <cstring>
#include <memory>
#include "Log.hpp"

struct MultipartPart {
    std::string headers;
    std::string name;
    std::string filename;
    std::string content_type;
    std::string data;
};

std::string					    getImfFixdate();
std::string					    trimWhitespace(std::string_view	sv);
std::vector<std::string>	    splitStringView(std::string_view sv, std::string_view delim = " ");
std::vector<std::string>	    splitUri(std::string_view uri);
bool						    isValidIPv4(std::string_view sv);
bool						    resourceExists(std::string_view uri, std::string searchDir = "");
bool						    uriFormatOk(std::string_view uri);
bool						    uriTargetAboveRoot(std::string_view uri);
bool						    isValidPort(std::string_view sv);
bool						    isValidImfFixdate(std::string_view sv);
bool						    isUnsignedIntLiteral(std::string_view sv);
bool						    isPositiveDoubleLiteral(std::string_view sv);

std::string					    getFileAsString(std::string const &fileName, std::string searchDir = "");
std::string					    getAbsPath(std::string const &fileName, std::string searchDir = "");

std::string                     extract_value(const std::string& source, const std::string& key);
std::string                     extract_quoted_value(const std::string& source, const std::string& key);
void                            save_to_disk(const MultipartPart& part, std::ofstream& outfile);
std::unique_ptr<std::ofstream>  initial_save_to_disk(const MultipartPart& part);