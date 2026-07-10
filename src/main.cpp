#include <asio.hpp>       
#include <sodium.h>
#include <cstdio>
#include <cstdlib>

int main() {
    // 0 : success, first call
    // 1 : success, already initialized
    // -1 : failure
    if (sodium_init() == -1) {
        std::cerr << "libsodium failed to initialize\n";
        return EXIT_FAILURE;
    }

    asio::io_context io_context;

    std::cout << "LAN Drop starting up...\n";
    std::cout << "libsodium initialized successfully.\n";
    std::cout << "Asio io_context constructed successfully.\n";

    return EXIT_SUCCESS;
}