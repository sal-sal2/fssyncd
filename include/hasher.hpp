#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <sodium.h>

namespace fssyncd {

// A hash of a file's bytes used to detect whether a file's contents changed
using Hash = std::array<uint8_t, crypto_generichash_BYTES>;

Hash hash_bytes(const std::vector& data);
}