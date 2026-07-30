#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sodium.h>

namespace landrop {

class ICryptoStrategy {
public:
    virtual ~ICryptoStrategy() = default;

    // Encrypts plaintext data and returns it as one:
    // [ 24 bytes nonce ] [ Encrypted data ] [ 16 bytes auth tag ]
    virtual std::vector<uint8_t> encrypt(std::span<const uint8_t> plaintext) = 0;

    // Decrypts data produced by encrypt. 
    // If the data was corrupted, modified, or encrypted with wrong key it returns 'std::nullopt'
    // or the decrypted byte vector on success.
    virtual std::optional<std::vector<uint8_t>> decrypt(std::span<const uint8_t> ciphertext) = 0;
};

// Real encryption (uses XChaCha20-Poly1305 AEAD)
class AesCryptoStrategy : public ICryptoStrategy {
public:
    // The key never changes size, so this doesn't own a std::vector -- a
    // fixed-size std::array communicates "this is always exactly N bytes"
    // both to the compiler and to anyone reading the constructor.
    explicit AesCryptoStrategy(std::span<const uint8_t> pre_shared_key);

    std::vector<uint8_t> encrypt(std::span<const uint8_t> plaintext) override;
    std::optional<std::vector<uint8_t>> decrypt(std::span<const uint8_t> ciphertext) override;

private:
    // Stores the encryption key inside the object, they key has constant size
    std::array<uint8_t, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> key_;
};

// Testing encryption to prevent high compute
class PlainTextStrategy : public ICryptoStrategy {
public:
    // Returns copy of the data as is
    std::vector<uint8_t> encrypt(std::span<const uint8_t> plaintext) override;

    // Wraps the data in std::optional and returns it
    std::optional<std::vector<uint8_t>> decrypt(std::span<const uint8_t> ciphertext) override;
};

}
