#include "Parser.hpp"

Parser::Parser(const std::string& file_name)
    :_file_name(file_name), _file() {
        std::error_code ec;
        /**
         * Try to locate the file in the current file system, will throw an error
         * if file is not exsists
        */
        if(!std::filesystem::exists(_file_name, ec) || ec) {
            throw ParserException(
                "Error : configuration file not exist <" + _file_name + ">"
            );
        }
        /**
         * Will compare the file extesnsion with the standard one and will throw
         * an error in case mismatch. Subsequent characters in the file_name after
         * the last occurence of '.'
        */
        size_t pos = _file_name.rfind('.');
        std::string ext = _file_name.substr(pos + 1);
        if(ext != EXTENSION) {
            throw ParserException(
                "Error : wrong configuration file extension <" + _file_name + ">"
            );
        }

        _file.open(_file_name);

        /**
         * if the file pointed by the file_name can not open, will throw an error
        */
        if(_file.fail()) {
            throw ParserException(
                "Error : can not open configuration file <" + _file_name + ">"
            );
        }

        /**
         * If the file pointed by the file_name is empty, will throw and error
         */
        if(std::filesystem::file_size(_file_name) == 0) {
            throw ParserException(
                "Error : empty configutaion file <" + _file_name + ">"
            );
        }
    /**
     * Sucessfully opening the file and tokenizing the content
    */
    tokenize_file();
}

Parser::~Parser() {
    /**
     * At the end if the file descriptor is still open, it will  be closed gracefully
    */
    if(_file.is_open()) {
        _file.close();
    }
}

/**
 * Will open the configuration file and read line by line, every line will be break down
 * to tokens base on spaces/tabs
 * @param void - class method will have access to all the class attributes
 * @return void - all the token will save to an internal container
*/
void Parser::tokenize_file(void) {
    std::string line;

    while(getline(_file, line)) {
        std::stringstream ss(line);
        std::string word;
        while(ss >> word) {
            _tokens.push_back(word);
            word.clear();
        }
        line.clear();
    }
}

void Parser::print_tokens(void) {
    for(auto it : _tokens){
        std::cout << it << "\n";
    }
}

/**
 * Changed return type to reference (Sonja)
 */
config_t& Parser::getServerConfig(const std::string& host_name) {
    (void)host_name;

    redirect_t  temp_redirect;

    route_t     temp_route1;
    route_t     temp_route2;
    route_t     temp_route3;
    route_t     temp_route4;
    route_t     temp_route5;

    config_t    dummy_config;

    /**
     *  creating dummy server config
     */
    dummy_config.host                   = "127.0.0.1";
    dummy_config.port                   = 9304;
    dummy_config.client_max_body_size   = 1048576; // equal to 1MB

    dummy_config.server_names.clear();
    dummy_config.server_names.push_back("localhost");
    dummy_config.server_names.push_back("webserv.local");

    dummy_config.error_pages.clear();
    dummy_config.error_pages.emplace(static_cast<uint16_t>(403), "/errors/403.html");
    dummy_config.error_pages.emplace(static_cast<uint16_t>(404), "/errors/404.html");
    dummy_config.error_pages.emplace(static_cast<uint16_t>(500), "/errors/500.html");

    /**
     * dummy redirect values
     */
    redirect_t no_redirect;
    no_redirect.status_code = 0;
    no_redirect.target_url  = "";

    /**
     * dummy route 01
    */
    temp_route1.path                   = "/";
    temp_route1.abs_path               = "./www";
    temp_route1.accepted_methods       = { "GET" };
    temp_route1.redirect               = no_redirect;
    temp_route1.auto_index             = false;
    temp_route1.index_file             = "index.html";
    temp_route1.cgi_extensions         = {};
    temp_route1.cgi_methods            = {};
    temp_route1.upload_path            = "";
    temp_route1.cgi_executable         = "";
    temp_route1.client_max_body_size   = 1048576; // equal to 1MB

    /**
     * dummy route 02
    */
    temp_route2.path                   = "/images";
    temp_route2.abs_path               = "./www/images";
    temp_route2.accepted_methods       = { "GET" };
    temp_route2.redirect               = no_redirect;
    temp_route2.auto_index             = true;
    temp_route2.index_file             = "";
    temp_route2.cgi_extensions         = {};
    temp_route2.cgi_methods            = {};
    temp_route2.upload_path            = "";
    temp_route2.cgi_executable         = "";
    temp_route2.client_max_body_size   = 1048576; // equal to 1MB

    /**
     * dummy route 03
    */
    temp_route3.path                   = "/upload";
    temp_route3.abs_path               = "./www/uploads";
    temp_route3.accepted_methods       = { "POST" };
    temp_route3.redirect               = no_redirect;
    temp_route3.auto_index             = false;
    temp_route3.index_file             = "";
    temp_route3.cgi_extensions         = {};
    temp_route3.cgi_methods            = {};
    temp_route3.upload_path            = "./www/uploads";
    temp_route3.cgi_executable         = "";
    temp_route3.client_max_body_size   = 5242880; // equal to 5MB

    /**
     * dummy route 04
    */
    temp_route4.path                   = "/cgi";
    temp_route4.abs_path               = "./www/cgi-bin";
    temp_route4.accepted_methods       = { "GET", "POST" };
    temp_route4.redirect               = no_redirect;
    temp_route4.auto_index             = false;
    temp_route4.index_file             = "";
    temp_route4.cgi_extensions         = { ".py", ".php" };
    temp_route4.cgi_methods            = { "GET", "POST" };
    temp_route4.upload_path            = "";
    temp_route4.cgi_executable         = "/usr/bin/python3";
    temp_route4.client_max_body_size   = 2097152; // equal to 2MB

    /**
     * dummy route 05
    */
    temp_redirect.status_code          = 301;
    temp_redirect.target_url           = "/";
    temp_route5.path                   = "/old";
    temp_route5.abs_path               = "";
    temp_route5.accepted_methods       = { "GET" };
    temp_route5.redirect               = temp_redirect;
    temp_route5.auto_index             = false;
    temp_route5.index_file             = "";
    temp_route5.cgi_extensions         = {};
    temp_route5.cgi_methods            = {};
    temp_route5.upload_path            = "";
    temp_route5.cgi_executable         = "";
    temp_route5.client_max_body_size   = 2097152; // equal to 2MB

    /**
     * attached routes to srever config
    */
    dummy_config.routes.clear();
    dummy_config.routes.emplace(static_cast<uint16_t>(1), temp_route1);
    dummy_config.routes.emplace(static_cast<uint16_t>(2), temp_route2);
    dummy_config.routes.emplace(static_cast<uint16_t>(3), temp_route3);
    dummy_config.routes.emplace(static_cast<uint16_t>(4), temp_route4);
    dummy_config.routes.emplace(static_cast<uint16_t>(5), temp_route5);

    /**
     * adding server to class standard server container
    */
    _server_configs.emplace_back(dummy_config);

    return dummy_config;
}

std::vector<config_t> const& Parser::getServerConfigs() const
{
	return _server_configs;
}
