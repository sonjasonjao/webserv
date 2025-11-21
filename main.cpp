#include <iostream>

#include "Parser.hpp"
#include "CustomeException.hpp"

int main(int argc, char* argv[]) {
    if(argc == 2) {
        std::string file_name(argv[1]);
        try {
            Parser p(file_name);
            p.print_tokens();
            /**
             * you can get the dummy config date as shown below
            */
            config_t conf = p.getServerConfig("127.0.0.1");
            // testing that this is working
            std::cout << "configurred port for 127.0.0.1 : " << conf.port << "\n";
        }catch (const std::exception& e ){
            std::cerr << e.what() << "\n";
        }
    } else {
        std::cerr << "Invalid arguments!" << "\n";
    }

    return (EXIT_SUCCESS);
}
