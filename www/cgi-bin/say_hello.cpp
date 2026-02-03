#include <iostream>
#include <cstdint>

int main(void) {
    std::cout << "Content-Type: text/plain\r\n";
    std::cout << "\r\n";
    std::cout << "Regardless of what is requesting......." << "\n";
    std::cout << "... I am saying HELLO MY FRIEND!" << "\n";
    return 0;
}