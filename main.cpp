#include <iostream>

#include "Parser.hpp"
#include "CustomeException.hpp"

int main(int argc, char* argv[]) {
    if(argc == 2) {
        std::cout << argv[1] << " file will be open :) \n";
        std::string temp = "Hellooooooooooooooo";
        Parser p(temp);
        try {
            std::cout << p.getMessage() <<"\n";
        }catch (const std::exception& e ){
            std::cerr << e.what() << "\n";
        }
    } else {
        std::cerr << "Invalid arguments!" << "\n";
    }

    return (EXIT_SUCCESS);
}
