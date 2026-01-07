#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "CustomException.hpp"
#include "JSON.hpp"
#include "../include/Log.hpp"

/**
 * Default file_name extension for the configuration file, with out correct extension
 * file_name invalid.
*/
#define EXTENSION "json"

struct Redirect {
    uint16_t                            status_code;            // http status code for redirect (eg : 301, 302, 303, etc)
    std::string                         target_url;             // Target url which client will redirect
};

struct Route {
    std::string                         path;                   // URI prefix for the route, e.g. "/", "/images/", "/upload"
    std::string                         abs_path;               // Absolute filesystem path corresponding the route's root eg : "/var/www/site"
    std::vector<std::string>            accepted_methods;       // list of allowed http methds eg : GET, POST, DELETE
    Redirect                            redirect;               // This will serve if the request is redirect
    bool                                auto_index;             // if true, enable directory listing when the requested path is a directory
    std::string                         index_file;             // Default file name to serve
    std::vector<std::string>            cgi_extensions;         // File extensions that should be handled by a CGI eg ".php, .py"
    std::vector<std::string>            cgi_methods;            // http methods allowed specifically for CGI execution
    std::string                         upload_path;            // Directory where uploaded files handled by route should store
    std::string                         cgi_executable;         // Path to the CGI interpreter/executable to run for matching files
    size_t                              client_max_body_size;   // Maximum allowed size (in bytes) of the request body for this route.
};

struct Config {
    std::string                         host;                   // IP or hostname on which this server listens, e.g. "0.0.0.0" or "127.0.0.1"
    std::vector<uint16_t>               ports;                   // Port on which this server listens
    std::string                         host_name;           // List of server names (virtual hosts) handled by this server eg : {"example.com", "www.example.com"}
    // will uncomment when we ready to use below
    std::map<std::string, std::string>	status_pages;				// Mapping from HTTP status code to custom page path.
    std::map<std::string, std::string>	routes;						// Set of routes (URI -> path definitions) for this server.
    //size_t                              client_max_body_size;		// Default maximum allowed size (in bytes) of the request body for this server
};

class Parser {
    private:
        std::string const               _file_name;             // File name of the configuratioon file
        std::ifstream                   _file;                  // ifstream instance to read the configuration file
        std::vector<Config>             _server_configs;        // List of fully parsed server configurations built from the token list.

    public:
        /**
         * proper-way of creating Parser instance
         * @param std::string file_name - file name need to parse
         * @return void - content of the file will be tokenize and save to a internal
         * container, type std::vector
        */
        Parser(const std::string& file_name);
        /**
         * only way of creating Parser instanse should be argument constructor
        */
        Parser() = delete;
        Parser(const Parser& other) = delete;
        Parser& operator=(const Parser& other) = delete;
        /**
         * destructor will take care of releasing resources
        */
        ~Parser();

        /**
         * All the exceptions in Parsing will be categorize under ParserException
         * and will carry a string describing what went wrong
        */
        struct ParserException : public CustomException {
            ParserException(const std::string& str):
                CustomException(str){}
        };

        /**
         * Correct configuration file will be read line by line, every line will be tokenize
         * and svaed to the _tokens
         * @param void
         * @return void
        */
        void tokenizeFile(void);

        /**
         * First version of getter method to get a final srever configuration information. As the first
         * version only return some dummy values to start implementing Main loop for Sonja
        */
       const Config& getServerConfig(size_t index);

	   const std::vector<Config>	&getServerConfigs(void) const;

       size_t getNumberOfServerConfigs(void);

       Config convertToServerData(const Token& server);
};
