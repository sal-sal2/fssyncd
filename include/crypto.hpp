#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sodium.h>

namespace fssyncd {

class Crypto {
public:    
    explicit Crypto(std::span<const uint8_t> pre_shared_key);

    // Encrypts plaintext data and returns it as one:
    // [24 bytes nonce][Encrypted data][16 bytes auth tag]
    std::vector<uint8_t> encrypt(std::span<const uint8_t> plaintext);
    std::optional<std::vector<uint8_t>> decrypt(std::span<const uint8_t> ciphertext);

private:
    // Stores the encryption key inside the object, they key has constant size
    std::array<uint8_t, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> key_;
};
}
