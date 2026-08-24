#include "landrop/crypto.hpp"

#include <stdexcept>

namespace fssyncd {

constexpr size_t NONCE_BYTES = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;  // 24
constexpr size_t TAG_BYTES = crypto_aead_xchacha20poly1305_ietf_ABYTES;       // 16

Crypto::Crypto(std::span<const uint8_t> pre_shared_key) {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium failed to initialize");
    }

    if (pre_shared_key.size() != key_.size()) {
        throw std::runtime_error("pre-shared key must be exactly " + std::to_string(key_.size()) + " bytes");
    }

    std::copy(pre_shared_key.begin(), pre_shared_key.end(), key_.begin());
}

std::vector<uint8_t> Crypto::encrypt(std::span<const uint8_t> plaintext) {

    std::vector<uint8_t> output(NONCE_BYTES + plaintext.size() + TAG_BYTES);

    uint8_t* nonce = output.data();

    randombytes_buf(nonce, NONCE_BYTES);

    uint8_t* ciphertext = output.data() + NONCE_BYTES;
    unsigned long long ciphertext_len = 0;

    int result = crypto_aead_xchacha20poly1305_ietf_encrypt(
        ciphertext, &ciphertext_len,
        plaintext.data(), plaintext.size(),
        nullptr, 0,
        nullptr,
        nonce,
        key_.data());

    if (result != 0) {
        throw std::runtime_error("encrypt failed");
    }

    return output;
}

std::optional<std::vector<uint8_t>> Crypto::decrypt(std::span<const uint8_t> ciphertext) {
    if (ciphertext.size() < NONCE_BYTES + TAG_BYTES) {
        return std::nullopt;
    }

    const uint8_t* nonce = ciphertext.data();

    const uint8_t* sealed_data = ciphertext.data() + NONCE_BYTES;
    size_t sealed_data_len = ciphertext.size() - NONCE_BYTES;

    std::vector<uint8_t> plaintext(sealed_data_len - TAG_BYTES);
    unsigned long long plaintext_len = 0;

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
}