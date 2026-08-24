#include "landrop/message.hpp"

#include <algorithm>
#include <type_traits>

namespace fssyncd {

namespace {
// Helper class to construct a vector of binary bytes 
struct ByteWriter {
    std::vector<uint8_t> buffer;

    void write_u8(uint8_t value) {
        buffer.push_back(value);
    }

    void write_u32(uint32_t value) {
        buffer.push_back(static_cast<uint8_t>(value >> 24));
        buffer.push_back(static_cast<uint8_t>(value >> 16));
        buffer.push_back(static_cast<uint8_t>(value >> 8));
        buffer.push_back(static_cast<uint8_t>(value));
    }

    void write_u64(uint64_t value) {
        buffer.push_back(static_cast<uint8_t>(value >> 56));
        buffer.push_back(static_cast<uint8_t>(value >> 48));
        buffer.push_back(static_cast<uint8_t>(value >> 40));
        buffer.push_back(static_cast<uint8_t>(value >> 32));
        buffer.push_back(static_cast<uint8_t>(value >> 24));
        buffer.push_back(static_cast<uint8_t>(value >> 16));
        buffer.push_back(static_cast<uint8_t>(value >> 8));
        buffer.push_back(static_cast<uint8_t>(value));
    }

    void write_bytes(const uint8_t* data, size_t length) {
        buffer.insert(buffer.end(), data, data + length);
    }

};

// Helper class to extract data types form binary bytes
struct ByteReader {
    std::span<const uint8_t> buffer;
    size_t cursor = 0;
    bool failed = false;

    bool has_bytes_remaining(size_t count) {
        if (failed || count > buffer.size() - cursor) {
            // Flag stays true for future reads
            failed = true;
            return false;
        }
        return true;
    }
    uint8_t read_u8() {
        if (!has_bytes_remaining(1)) {
            return 0;
        }
        uint8_t value = buffer[cursor];
        cursor += 1;
        return value;
    }

    uint32_t read_u32() {
        if (!has_bytes_remaining(4)) {
            return 0;
        }
        uint32_t value = (static_cast<uint32_t>(buffer[cursor]) << 24) | 
                         (static_cast<uint32_t>(buffer[cursor + 1]) << 16) |
                         (static_cast<uint32_t>(buffer[cursor + 2]) << 8) |
                          static_cast<uint32_t>(buffer[cursor + 3]);
        cursor += 4;
        return value;
    }

    uint64_t read_u64() {
        if (!has_bytes_remaining(8)) {
            return 0;
        }
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | static_cast<uint64_t>(buffer[cursor + i]);
        }
        cursor += 8;
        return value;
    }

    std::vector<uint8_t> read_bytes(size_t length) {
        if (!has_bytes_remaining(length)) {
            return {};
        }
        std::vector<uint8_t> result(buffer.begin() + cursor, buffer.begin() + cursor + length);
        cursor += length;
        return result;
    }

};

constexpr uint8_t HANDSHAKE_TAG = 0x00;
constexpr uint8_t FILE_META_DATA_TAG = 0x01;
constexpr uint8_t FILE_CHUNK_TAG = 0x02;


void write_handshake(ByteWriter& writer, const Handshake& handshake) {
    writer.write_u8(HANDSHAKE_TAG);
    writer.write_u32(handshake.protocol_version);
    writer.write_bytes(handshake.session_nonce.data(), handshake.session_nonce.size());
}

std::optional<Handshake> read_handshake(ByteReader& reader) {
    Handshake handshake;
    handshake.protocol_version = reader.read_u32();

    std::vector<uint8_t> nonce_bytes = reader.read_bytes(handshake.session_nonce.size());
    if (reader.failed) {
        return std::nullopt;
    }
    std::copy(nonce_bytes.begin(), nonce_bytes.end(), handshake.session_nonce.begin());

    return handshake;
}

void write_file_metadata(ByteWriter& writer, const FileMetadata& metadata) {
    writer.write_u8(FILE_META_DATA_TAG);

    writer.write_u32(static_cast<uint32_t>(metadata.path.size()));
    writer.write_bytes(reinterpret_cast<const uint8_t*>(metadata.path.data()), metadata.path.size());

    writer.write_u64(metadata.file_size);
    writer.write_u32(metadata.chunk_count);
}

std::optional<FileMetadata> read_file_metadata(ByteReader& reader) {
    FileMetadata metadata;

    uint32_t path_length = reader.read_u32();
    std::vector<uint8_t> path_bytes = reader.read_bytes(path_length);
    metadata.path = std::string(path_bytes.begin(), path_bytes.end());

    metadata.file_size = reader.read_u64();
    metadata.chunk_count = reader.read_u32();

    if (reader.failed) {
        return std::nullopt;
    }
    return metadata;
}

void write_file_chunk(ByteWriter& writer, const FileChunk& chunk) {
    writer.write_u8(FILE_CHUNK_TAG);
    writer.write_u32(chunk.chunk_index);

    writer.write_u32(static_cast<uint32_t>(chunk.data.size()));
    writer.write_bytes(chunk.data.data(), chunk.data.size());
}

std::optional<FileChunk> read_file_chunk(ByteReader& reader) {
    FileChunk chunk;
    chunk.chunk_index = reader.read_u32();

    uint32_t data_length = reader.read_u32();
    chunk.data = reader.read_bytes(data_length);

    if (reader.failed) {
        return std::nullopt;
    }
    return chunk;
}

}  

std::vector<uint8_t> serialize(const Message& message) {
    ByteWriter writer;

    if (std::holds_alternative<Handshake>(message)) {
        write_handshake(writer, std::get<Handshake>(message));
    }
    else if (std::holds_alternative<FileMetadata>(message)) {
        write_file_metadata(writer, std::get<FileMetadata>(message));
    }
    else if (std::holds_alternative<FileChunk>(message)) {
        write_file_chunk(writer, std::get<FileChunk>(message));
    }

    return std::move(writer.buffer);
}

std::optional<Message> deserialize(std::span<const uint8_t> buffer) {
    ByteReader reader{buffer};

    uint8_t tag = reader.read_u8();
    if (reader.failed) {
        return std::nullopt;
    }

    switch (tag) {
        case HANDSHAKE_TAG: {
            std::optional<Handshake> handshake = read_handshake(reader);
            if (!handshake) {
                return std::nullopt;
            }
            return Message(*handshake);
        }
        case FILE_META_DATA_TAG: {
            std::optional<FileMetadata> metadata = read_file_metadata(reader);
            if (!metadata) {
                return std::nullopt;
            }
            return Message(*metadata);
        }
        case FILE_CHUNK_TAG: {
            std::optional<FileChunk> chunk = read_file_chunk(reader);
            if (!chunk) {
                return std::nullopt;
            }
            return Message(*chunk);
        }
        default:
            // Unknown tag byte, likely a modified message
            return std::nullopt;
    }
}

}
