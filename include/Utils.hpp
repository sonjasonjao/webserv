#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <memory>

/**
 * @struct MultipartPart
 * @brief Represents an individual entity within a multipart/form-data payload.
 *
 * This structure stores both the metadata (headers) and the payload (data)
 * for a single part of a multipart message.
 *
 * FIELDS:
 * - headers:      The raw HTTP header string for this part.
 * - name:         The form-field name (from Content-Disposition 'name').
 * - filename:     The client-side name of the file (from 'filename'), if provided.
 * - content_type: The MIME type of the data (e.g., "text/plain" or "image/png").
 * - data:         The raw content or binary body of the part.
 */

struct MultipartPart {
    std::string headers;
    std::string name;
    std::string filename;
    std::string contentType;
    std::string data;
};

std::vector<std::string>	splitStringView(std::string_view sv, std::string_view delim = " ");
std::vector<std::string>	splitUri(std::string_view uri);

std::string	trimWhitespace(std::string_view	sv);
std::string	getImfFixdate();
std::string	getFileAsString(std::string const &fileName, std::string searchDir = "");
std::string	getAbsPath(std::string const &fileName, std::string searchDir = "");

bool	isValidIPv4(std::string_view sv);
bool	resourceExists(std::string_view uri, std::string searchDir = "");
bool	uriFormatOk(std::string_view uri);
bool	uriTargetAboveRoot(std::string_view uri);
bool	isValidPort(std::string_view sv);
bool	isValidImfFixdate(std::string_view sv);
bool	isUnsignedIntLiteral(std::string_view sv);
bool	isPositiveDoubleLiteral(std::string_view sv);

std::unique_ptr<std::ofstream>	initialSaveToDisk(const MultipartPart& part, const std::string& path);
std::string						extractValue(const std::string& source, const std::string& key);
std::string						extractQuotedValue(const std::string& source, const std::string& key);
bool							saveToDisk(const MultipartPart& part, std::ofstream& outfile);
