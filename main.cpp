#include <iostream>

#include "Parser.hpp"
#include "CustomeException.hpp"

int main(int argc, char* argv[]) {
    if(argc == 2) {
        std::string file_name(argv[1]);
        try {
            Parser p(file_name);
            size_t no_of_coonfigs = p.getNumberOfServerConfigs();
            config_t cfg;
            for(size_t i = 0; i < no_of_coonfigs; ++i) {
                cfg = p.getServerConfig(i);
                std::cout << "Host : " << cfg.host << "\n";
                std::cout << "Host Name : " << cfg.host_name << "\n";
                size_t n = 1;
                std::cout << "Listening on : " << "\n";
                for(auto it : cfg.ports) {
                    std::cout << "Port [" << n << "] : " << it << "\n";
                    ++n;
                }
                std::cout << "-------------------------------- \n";
            }

        }catch (const std::exception& e ){
            std::cerr << e.what() << "\n";
        }
    } else {
        std::cerr << "Invalid arguments!" << "\n";
    }

    return (EXIT_SUCCESS);
}
