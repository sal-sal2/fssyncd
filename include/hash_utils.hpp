#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <sodium.h>

namespace fssyncd {

// Computes a fixed size hash for the input data buffer using libsodium's crypto_generichash
// crypto_generichash_BYTES: 32 bytes
std::array<uint8_t, crypto_generichash_BYTES> compute_hash(std::span<const uint8_t> data);

}