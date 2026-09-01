#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <sodium.h>

namespace fssyncd {

inline constexpr size_t KEY_BYTES = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
using EncryptionKey = std::array<uint8_t, KEY_BYTES>;

class Encryptor {
public:    
    explicit Encryptor(const EncryptionKey& key);

    // Encrypts/decrypts file chunks with an AEAD cipher
    // [24 bytes nonce][Encrypted data][16 bytes auth tag]
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext) const;
    std::optional<std::vector<uint8_t>> decrypt(const std::vector<uint8_t>& ciphertext) const;

private:
    // Stores the encryption key inside the object, they key has constant size
    EncryptionKey key_;
};
}
