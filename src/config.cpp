#include "include/config.hpp"

#include <arpa/inet.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace fssyncd {

namespace {

namespace fs = std::filesystem;

// Trims leading and trailing whitespace
std::string trim(const std::string& text) {
    // Finds first character that is not a space, tab or newline
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// Reads a file line by line and parses key-value entries (around the '=') into a map
std::unordered_map<std::string, std::string> parse_key_value_lines(const std::string& path) {
    std::ifstream config_file(path);
    if (!config_file) {
        throw std::runtime_error("could not open config file: " + path);

    }

    std::unordered_map<std::string, std::string> values;
    std::string line;

    // Traverse each line in the file
    while (std::getline(config_file, line)) {
        std::string trimmed_line = trim(line);
        if (trimmed_line.empty() || trimmed_line.front() == '#') {
            continue;
        }

        size_t equals_position = trimmed_line.find('=');
        if (equals_position == std::string::npos) {
            throw std::runtime_error("config line is not in 'key = value' form: " + trimmed_line);
        }
        std::string key = trim(trimmed_line.substr(0, equals_position));
        std::string value = trim(trimmed_line.substr(equals_position + 1));
        values[key] = value;
    }

    return values;
}

std::string require_value(const std::unordered_map<std::string, std::string>& values, const std::string& key, const std::string& config_path) {
    auto entry = values.find(key);
    if (entry == values.end() || entry->second.empty()) {
        throw std::runtime_error("config file " + config_path + " is missing required setting: " + key);
    }
    return entry->second;
}

// Ensures the watched directory path exists or creates it
void validate_watched_directory(const std::string& directory_path) {
    std::error_code error;

    // Checks path existence and create folder
    if (!fs::exists(directory_path, error)) {
        fs::create_directories(directory_path, error);
        if (error) {
            throw std::runtime_error("watched directory does not exist and could not be created: " + directory_path + " (" + error.message() + ")");
        }
        return;
    }

    // Verify the path is a folder not file
    if (!fs::is_directory(directory_path, error)) {
        throw std::runtime_error("watched directory path exists but is not a directory: " + directory_path);
    }
}

uint8_t hex_digit_to_digit(char digit) {
    if (digit >= '0' && digit <= '9') return static_cast<uint8_t>(digit - '0');
    if (digit >= 'a' && digit <= 'f') return static_cast<uint8_t>(digit - 'a' + 10);
    if (digit >= 'A' && digit <= 'F') return static_cast<uint8_t>(digit - 'A' + 10);
    throw std::runtime_error("preshared key file contains a non-hex character");
}

EncryptionKey load_pre_shared_key(const std::string& key_path) {
    std::ifstream key_file(key_path);
    if (!key_file) {
        throw std::runtime_error("could not open pre-shared key file: " + key_path);

    }

    std::stringstream buffer;
    buffer << key_file.rdbuf();
    std::string hex_text = trim(buffer.str());

    constexpr size_t EXPECTED_HEX_CHARS = KEY_BYTES * 2;
    if (hex_text.size() != EXPECTED_HEX_CHARS) {
        throw std::runtime_error("pre-shared key file " + key_path + " must contain exactly " + std::to_string(EXPECTED_HEX_CHARS)
            + "hex characters (" + std::to_string(KEY_BYTES) + "bytes), found instead" + std::to_string(hex_text.size()));
        
    }

    EncryptionKey key{};
    for (size_t byte_index = 0; byte_index < KEY_BYTES; ++byte_index) {
        uint8_t high_digit = hex_digit_to_digit(hex_text[byte_index * 2]);
        uint8_t low_digit = hex_digit_to_digit(hex_text[byte_index * 2 + 1]);
        key[byte_index] = static_cast<uint8_t>((high_digit << 4) | low_digit);
    }

    return key;
}

void validate_peer_ip(const std::string& peer_ip) {
    unsigned char buffer[16];
    bool is_valid = (inet_pton(AF_INET, peer_ip.c_str(), buffer) == 1) ||
                    (inet_pton(AF_INET6, peer_ip.c_str(), buffer) == 1);
    if (!is_valid) {
        throw std::runtime_error("config 'peer_ip' is not a valid IP address: " + peer_ip);
    }
}

uint16_t parse_port(const std::string& field_name, const std::string& port_text) {
    int parsed_value = 0;

    try {
        parsed_value = std::stoi(port_text);
    } catch (const std::exception&) {
        throw std::runtime_error("config '" + field_name + "' is not a valid number: " + port_text);
    }

    if (parsed_value < 1 || parsed_value > 65535) {
        throw std::runtime_error("config '" + field_name + "' must be be between 1 and 65535, got: " + port_text);
    }

    return static_cast<uint16_t>(parsed_value);
}
}


Config load_config(const std::string& path) {
    std::unordered_map<std::string, std::string> raw_values = parse_key_value_lines(path);

    Config config;
    config.watched_directory = require_value(raw_values, "watched_directory", path);
    std::string key_path = require_value(raw_values, "pre_shared_key_path", path);
    std::string port_text = require_value(raw_values, "port", path);
    config.peer_ip = require_value(raw_values, "peer_ip", path);
    std::string peer_port_text = require_value(raw_values, "peer_port", path);

    // Check the extracted configuration settings
    validate_watched_directory(config.watched_directory);
    config.pre_shared_key = load_pre_shared_key(key_path);
    config.tcp_port = parse_port("port", port_text);
    config.peer_port = parse_port("peer_port", peer_port_text);
    validate_peer_ip(config.peer_ip);

    return config;
}

}