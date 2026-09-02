#include "include/config.hpp"
#include "include/sync_daemon.hpp"

#include <sodium.h>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <csignal>
#include <thread>


namespace {

volatile std::sig_atomic_t g_shutdown_requested = 0;
void handle_shutdown_signal(int /*signal_number*/) {
    g_shutdown_requested = 1;
}
}

int main(int argc, char** argv) {
    if (sodium_init() == -1) {
        std::cerr << "libsodium failed to initialize\n";
        return EXIT_FAILURE;
    }

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << "<config-file>\n";
        return EXIT_FAILURE;
    }

    fssyncd::Config config;
    try {
        config = fssyncd::load_config(argv[1]);
    } catch(const std::exception& error) {
        std::cerr << "invalid config file" << e.what() << '\n';
        return EXIT_FAILURE;
    }
    
    // Register the handler for ctrl+c and process termination signal
    std::signal(SIGINT, handle_shutdown_signal);
    std::signal(SIGTERM, handle_shutdown_signal);

    fssyncd::SyncDaemon daemon(std::move(config));
    daemon.start();

    while (!g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\nfssyncd received a shutdown signal, stoping...\n";
    daemon.stop();

    return EXIT_SUCCESS;
}