#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fssyncd {
// Data is sent as a Message
// Handshake: [1 byte tag][4 byte protocol version]
// FileMetadata: [1 byte tag][n byte file name][8 byte file size][4 byte chunk count]
// FileChunk: [1 byte tag][4 byte chunk count][n bytes file contents]


enum class MessageType: uint8_t {
    Handshake = 0,
    FileMetadata = 1,
    FileChunk = 2,
};

struct Message {
    MessageType type;

    // Handshake field
    uint32_t protocol_version = 0;

    // FileMetadata fields
    std::string file_name;
    uint64_t file_size = 0;
    uint32_t chunk_count = 0;

    // FileChunk fields
    uint32_t chunk_index = 0;
    std::vector<uint8_t> chunk_data;
};

// Converts Message object into list of bytes to send over the network
std::vector<uint8_t> serialize_message(const Message& message);

// Coverts list of bytes from the network back into a Message object
std::optional<Message> deserialize_message(const std::vector<uint8_t>& buffer);

}  
