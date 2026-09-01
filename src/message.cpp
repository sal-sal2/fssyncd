#include "include/message.hpp"
#include "include/byte_helper.hpp"

#include <algorithm>
#include <type_traits>

namespace fssyncd {

std::vector<uint8_t> serialize_message(const Message& message) {
    std::vector<uint8_t> out;
    out.push_back(static_cast<uint8_t>(message.type));

    switch (message.type) {
        case MessageType::Handshake:
            write_u32(out, message.protocol_version);
            break;
        
        case MessageType::FileMetadata:
            write_u32(out, static_cast<uint32_t>(message.file_name.size()));
            out.insert(out.end(), message.file_name.begin(), message.file_name.end());
            write_u64(out, message.file_size);
            write_u32(out, message.chunk_count);
            break;
        
        case MessageType::FileChunk:
            write_u32(out, message.chunk_index);
            write_u32(out, static_cast<uint32_t>(message.chunk_data.size()));
            out.insert(out.end(), message.chunk_data.begin(), message.chunk_data.end());
            break;
    }

    return out;
}

std::optional<Message> deserialize_message(const std::vector<uint8_t>& buffer, size_t offset) {
    if (buffer.empty()) {
        return std::nullopt;
    }

    Message message;
    message.type = static_cast<MessageType>(buffer[0]);
    size_t pos = 1;

    switch (message.type) {
        case MessageType::Handshake: {
            if (buffer.size() < pos + 4) {
                return std::nullopt;
            }
            message.protocol_version = read_u32(buffer, pos);
            pos += 4;
            break;
        }
        case MessageType::FileMetadata: {
            if (buffer.size() < pos + 4) {
                return std::nullopt;
            }
            uint32_t name_len = read_u32(buffer, pos);
            pos += 4;

            if (buffer.size() < pos + name_len) {
                return std::nullopt;
            }
            message.file_name.assign(buffer.begin() + pos, buffer.begin() + pos + name_len);
            pos += name_len;

            if (buffer.size() < pos + 8) {
                return std::nullopt;
            }
            message.file_size = read_u64(buffer, pos);
            pos += 8;

            if (buffer.size() < pos + 4) {
                return std::nullopt;
            }
            message.chunk_count = read_u32(buffer, pos);
            pos += 4;
            break;
        }

        case MessageType::FileChunk: {
            if (buffer.size() < pos + 4) {
                return std::nullopt;
            }
            message.chunk_index = read_u32(buffer, pos);
            pos += 4;

            if (buffer.size() < pos + 4) {
                return std::nullopt;
            }
            uint32_t data_len = read_u32(buffer, pos);
            pos += 4;

            if (buffer.size() < pos + data_len) {
                return std::nullopt;
            }
            message.chunk_data.assign(buffer.begin() + pos, buffer.begin() + pos + data_len);
            pos += data_len;
            break;
        }
        
        default:
            std::nullopt;
    }

    return message;
}
}
