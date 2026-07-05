#include <cstdlib>
#include <iostream>

#include <asio.hpp>
#include <sodium.h>

int main() {
    // 0 : success, first call
    // 1 : success, already initialized
    // -1 : failure
    if (sodium_init() < 0) {
        std::cerr << "libsodium failed to initialize\n";
        return EXIT_FAILURE;
    }

    asio::io_context io_context;


    // Output
    std::cout << "==================================================\n";
    std::cout << " LAN Drop - starting up\n";
    std::cout << "==================================================\n";

    std::cout << "[lan-drop] is working.\n";

    return EXIT_SUCCESS;
}