#pragma once

#include "include/encryptor.hpp"

#include <cstdint>
#include <string>

namespace fssyncd {

// Holds all configured settings from the .conf file
struct Config {
    std::string watched_directory;
    EncryptionKey pre_shared_key{};

    uint16_t tcp_port = 0;
    std::string peer_ip;
    uint16_t peer_port = 0;
};

// Reads the config file, checks each field, decodes keys, and returns a Config struct 
Config load_config(const std::string& path);
}