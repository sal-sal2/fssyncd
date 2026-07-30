#include "landrop/hash_utils.hpp"

namespace landrop {

std::array<uint8_t, crypto_generichash_BYTES> compute_hash(std::span<const uint8_t> data) {
    std::array<uint8_t, crypto_generichash_BYTES> hashed{}

    crypto_generichash(hashed.data(), hashed.size(), data.data(), data.size(), nullptr, 0);

    return hashed;
}

}