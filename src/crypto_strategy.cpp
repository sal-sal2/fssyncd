#include "landrop/crypto_strategy.hpp"

#include <stdexcept>

namespace landrop {

namespace {
constexpr size_t kNonceBytes = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;  // 24
constexpr size_t kTagBytes = crypto_aead_xchacha20poly1305_ietf_ABYTES;       // 16
}

AesCryptoStrategy::AesCryptoStrategy(std::span<const uint8_t> pre_shared_key) {
    if (sodium_init() < 0) {
        throw std::runtime_error("AesCryptoStrategy: libsodium failed to initialize");
    }

    // Validate key length, it must match libsodium's (32 bytes)
    if (pre_shared_key.size() != key_.size()) {
        throw std::runtime_error("AesCryptoStrategy: pre-shared key must be exactly " + std::to_string(key_.size()) + " bytes");
    }

    // Copy the key bytes from the input span into the key_ array
    std::copy(pre_shared_key.begin(), pre_shared_key.end(), key_.begin());
}

std::vector<uint8_t> AesCryptoStrategy::encrypt(std::span<const uint8_t> plaintext) {

    std::vector<uint8_t> output(kNonceBytes + plaintext.size() + kTagBytes);

    // Get pointer to start of the vector where the nonce will be
    uint8_t* nonce = output.data();

    // Fill the first 24 bytes of output vector with random bytes form libsodium
    randombytes_buf(nonce, kNonceBytes);

    // Get pointer to start of the vector where the cipher will be written
    uint8_t* ciphertext = output.data() + kNonceBytes;
    unsigned long long ciphertext_len = 0;

    // Encyrpt using XChaCha20-Poly1305 AEAD
    int result = crypto_aead_xchacha20poly1305_ietf_encrypt(
        ciphertext, &ciphertext_len,
        plaintext.data(), plaintext.size(),
        nullptr, 0,
        nullptr,
        nonce,
        key_.data());

    if (result != 0) {
        throw std::runtime_error("AesCryptoStrategy::encrypt failed");
    }

    return output;
}

std::optional<std::vector<uint8_t>> AesCryptoStrategy::decrypt(std::span<const uint8_t> ciphertext) {
    // Check if encrypt produced valid output
    if (ciphertext.size() < kNonceBytes + kTagBytes) {
        return std::nullopt;
    }

    // Get to the 24 byte nonce at the beginning of the data
    const uint8_t* nonce = ciphertext.data();

    // Get to the starting position of the encrypted data
    const uint8_t* sealed_data = ciphertext.data() + kNonceBytes;
    size_t sealed_data_len = ciphertext.size() - kNonceBytes;

    std::vector<uint8_t> plaintext(sealed_data_len - kTagBytes);
    unsigned long long plaintext_len = 0;

    // Decyrpt using XChaCha20-Poly1305 AEAD
    int result = crypto_aead_xchacha20poly1305_ietf_decrypt(
        plaintext.data(), &plaintext_len,
        nullptr,
        sealed_data, sealed_data_len,
        nullptr, 0,
        nonce,
        key_.data());

    if (result != 0) {
        return std::nullopt;
    }

    return plaintext;
}

std::vector<uint8_t> PlainTextStrategy::encrypt(std::span<const uint8_t> plaintext) {
    return std::vector<uint8_t>(plaintext.begin(), plaintext.end());
}

std::optional<std::vector<uint8_t>> PlainTextStrategy::decrypt(std::span<const uint8_t> ciphertext) {
    return std::vector<uint8_t>(ciphertext.begin(), ciphertext.end());
}

}
