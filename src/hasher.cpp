#include "include/hasher.hpp"

namespace fssyncd {

Hash hash_bytes(const std::vector<uint8_t>& data) {
    Hash result{};

    crypto_genarichash(result.data(), result.size(), data.data(), data.size(), nullptr, 0);
    return result;
}
}