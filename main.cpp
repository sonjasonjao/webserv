#include <iostream>

#include "Parser.hpp"
#include "CustomeException.hpp"

int main(int argc, char* argv[]) {
    if(argc == 2) {
        std::string file_name(argv[1]);
        try {
            Parser p(file_name);
        }catch (const std::exception& e ){
            std::cerr << e.what() << "\n";
        }
    } else {
        std::cerr << "Invalid arguments!" << "\n";
    }

    return (EXIT_SUCCESS);
}
