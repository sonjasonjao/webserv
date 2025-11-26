#include <iostream>

#include "Parser.hpp"
#include "CustomeException.hpp"

int main(int argc, char* argv[]) {
    if(argc == 2) {
        std::string file_name(argv[1]);
        try {
            Parser p(file_name);
            config_t cfg = p.getServerConfig(0);
            std::cout << "Host : " << cfg.host << "\n";
            std::cout << "Host Name : " << cfg.host_name << "\n";
            std::cout << "Port : " << cfg.ports.at(0) << "\n";

            cfg = p.getServerConfig(1);
            std::cout << "Host : " << cfg.host << "\n";
            std::cout << "Host Name : " << cfg.host_name << "\n";
            std::cout << "Port : " << cfg.ports.at(0) << "\n";
            std::cout << "Port : " << cfg.ports.at(1) << "\n";
        }catch (const std::exception& e ){
            std::cerr << e.what() << "\n";
        }
    } else {
        std::cerr << "Invalid arguments!" << "\n";
    }

    return (EXIT_SUCCESS);
}
