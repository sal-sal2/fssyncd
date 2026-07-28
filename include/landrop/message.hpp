#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace landrop {
// Data delivery: [4-byte length][Message payload]
// Message tags: 0x00 = Handshake, 0x01 = FileMetaData, 0x02 = FileChunck

// Type sent after connecting to verify protocol version and crypto key
struct Handshake {
    uint32_t protocol_version;
    std::array<uint8_t, 24> session_nonce;
};

// Type sent before file transfer starts to describe the file
struct FileMetadata {
    std::string path;
    uint64_t file_size;
    uint32_t chunk_count;
};

// Type sent to repeatedly transmit the actual/encrypted file content
struct FileChunk {
    uint32_t chunk_index;
    std::vector<uint8_t> data;
};


using Message = std::variant<Handshake, FileMetadata, FileChunk>;

// Converts Message object into list of bytes to send over the network
std::vector<uint8_t> serialize(const Message& message);

// Coverts list of bytes from the network back into a Message object
std::optional<Message> deserialize(std::span<const uint8_t> buffer);

}  
