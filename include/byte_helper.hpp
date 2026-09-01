#pragma once

#include <vector>
#include <cstdint>


namespace fssyncd {

void write_u32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

void write_u64(std::vector<uint8_t>&out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>(value >> shift));
    }
}

uint32_t read_u32(const std::vector<uint8_t>& buffer, size_t offset) {
    return (static_cast<uint32_t>(buffer[offset]) << 24) | 
            (static_cast<uint32_t>(buffer[offset + 1]) << 16) |
            (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
            static_cast<uint32_t>(buffer[offset + 3]);
}

uint64_t read_u64(const std::vector<uint8_t>& buffer, size_t offset) {
    uint64_t value = 0;

    for (size_t i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<uint64_t>(buffer[offset + i]);
    }

    return value;
}
}