#include "landrop/message.hpp"

#include <algorithm>
#include <type_traits>

namespace landrop {

// ----- Helpers -----
namespace {
// Helper class to construct a vector of binary bytes 
class ByteWriter {
public:
    // Appends 1 byte to buffer
    void write_u8(uint8_t value) {
        buffer_.push_back(value);
    }

    // Appends a 32 bit int in Big Endian order by shifting off 1 byte at a time, most significant byte first
    void write_u32(uint32_t value) {
        buffer_.push_back(static_cast<uint8_t>(value >> 24));
        buffer_.push_back(static_cast<uint8_t>(value >> 16));
        buffer_.push_back(static_cast<uint8_t>(value >> 8));
        buffer_.push_back(static_cast<uint8_t>(value));
    }

    // Appends a 64 bit int in Big Endian order by shifting off 1 byte at a time, most significant byte first
    void write_u64(uint64_t value) {
        buffer_.push_back(static_cast<uint8_t>(value >> 56));
        buffer_.push_back(static_cast<uint8_t>(value >> 48));
        buffer_.push_back(static_cast<uint8_t>(value >> 40));
        buffer_.push_back(static_cast<uint8_t>(value >> 32));
        buffer_.push_back(static_cast<uint8_t>(value >> 24));
        buffer_.push_back(static_cast<uint8_t>(value >> 16));
        buffer_.push_back(static_cast<uint8_t>(value >> 8));
        buffer_.push_back(static_cast<uint8_t>(value));
    }

    // Appends a block of bytes to the buffer
    void write_bytes(const uint8_t* data, size_t length) {
        buffer_.insert(buffer_.end(), data, data + length);
    }

    // Moves and returns completed buffer to the caller
    std::vector<uint8_t> take_buffer() {
        return std::move(buffer_);
    }

private:
    std::vector<uint8_t> buffer_;
};

// Helper class to extract data types form binary bytes
class ByteReader {
public:
    explicit ByteReader(std::span<const uint8_t> buffer) : buffer_(buffer) {}

    bool failed() const {
        return failed_;
    }

    // Reads specified size of bytes to into into x-bit int
    uint8_t read_u8() {
        if (!has_bytes_remaining(1)) {
            return 0;
        }
        uint8_t value = buffer_[cursor_];
        cursor_ += 1;
        return value;
    }

    uint32_t read_u32() {
        if (!has_bytes_remaining(4)) {
            return 0;
        }
        uint32_t value = (static_cast<uint32_t>(buffer_[cursor_]) << 24) | 
                         (static_cast<uint32_t>(buffer_[cursor_ + 1]) << 16) |
                         (static_cast<uint32_t>(buffer_[cursor_ + 2]) << 8) |
                          static_cast<uint32_t>(buffer_[cursor_ + 3]);
        cursor_ += 4;
        return value;
    }

    uint64_t read_u64() {
        if (!has_bytes_remaining(8)) {
            return 0;
        }
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | static_cast<uint64_t>(buffer_[cursor_ + i]);
        }
        cursor_ += 8;
        return value;
    }

    // Copies 'length' bytes into a new vector
    std::vector<uint8_t> read_bytes(size_t length) {
        if (!has_bytes_remaining(length)) {
            return {};
        }
        std::vector<uint8_t> result(buffer_.begin() + cursor_, buffer_.begin() + cursor_ + length);
        cursor_ += length;
        return result;
    }

private:
    // Checks if we have enough space to read from the data to prevent reading past buffer
    bool has_bytes_remaining(size_t count) {
        if (failed_ || count > buffer_.size() - cursor_) {
            // Flag stays true for future reads
            failed_ = true;
            return false;
        }
        return true;
    }

    
    std::span<const uint8_t> buffer_;
    
    // Index of next (un)read byte
    size_t cursor_ = 0;
    bool failed_ = false;
};

// Message tags
constexpr uint8_t kHandshakeTag = 0x00;
constexpr uint8_t kFileMetadataTag = 0x01;
constexpr uint8_t kFileChunkTag = 0x02;


// Converts Handshake message
void write_handshake(ByteWriter& writer, const Handshake& handshake) {
    writer.write_u8(kHandshakeTag);
    writer.write_u32(handshake.protocol_version);
    writer.write_bytes(handshake.session_nonce.data(), handshake.session_nonce.size());
}

// Reads Handshake fields, after (0x00)
std::optional<Handshake> read_handshake(ByteReader& reader) {
    Handshake handshake;
    handshake.protocol_version = reader.read_u32();

    std::vector<uint8_t> nonce_bytes = reader.read_bytes(handshake.session_nonce.size());
    if (reader.failed()) {
        return std::nullopt;
    }
    std::copy(nonce_bytes.begin(), nonce_bytes.end(), handshake.session_nonce.begin());

    return handshake;
}

// Converts FileMetadata
void write_file_metadata(ByteWriter& writer, const FileMetadata& metadata) {
    writer.write_u8(kFileMetadataTag);

    // Write length prefix first for reader
    writer.write_u32(static_cast<uint32_t>(metadata.path.size()));
    writer.write_bytes(reinterpret_cast<const uint8_t*>(metadata.path.data()), metadata.path.size());

    writer.write_u64(metadata.file_size);
    writer.write_u32(metadata.chunk_count);
}

// Reads FileMetadata fields, after (0x01)
std::optional<FileMetadata> read_file_metadata(ByteReader& reader) {
    FileMetadata metadata;

    uint32_t path_length = reader.read_u32();
    std::vector<uint8_t> path_bytes = reader.read_bytes(path_length);
    metadata.path = std::string(path_bytes.begin(), path_bytes.end());

    metadata.file_size = reader.read_u64();
    metadata.chunk_count = reader.read_u32();

    if (reader.failed()) {
        return std::nullopt;
    }
    return metadata;
}

// Converts FileChunk
void write_file_chunk(ByteWriter& writer, const FileChunk& chunk) {
    writer.write_u8(kFileChunkTag);
    writer.write_u32(chunk.chunk_index);

    // Write length so reader knows size
    writer.write_u32(static_cast<uint32_t>(chunk.data.size()));
    writer.write_bytes(chunk.data.data(), chunk.data.size());
}

std::optional<FileChunk> read_file_chunk(ByteReader& reader) {
    FileChunk chunk;
    chunk.chunk_index = reader.read_u32();

    uint32_t data_length = reader.read_u32();
    chunk.data = reader.read_bytes(data_length);

    if (reader.failed()) {
        return std::nullopt;
    }
    return chunk;
}

}  

std::vector<uint8_t> serialize(const Message& message) {
    ByteWriter writer;

    if (std::holds_alternative<Handshake>(message)) {
        const Handshake& h = std::get<Handshake>(message);
        write_handshake(writer, h);
    }
    else if (std::holds_alternative<FileMetadata>(message)) {
        const FileMetadata& m = std::get<FileMetadata>(message);
        write_file_metadata(writer, m);
    }
    else if (std::holds_alternative<FileChunck>(message)) {
        const FileChunk& c = std::get<FileChunk>(message);
        write_file_chunk(writer, c);
    }

    return writer.take_buffer();
}

// Converts byte span back into a Message variant object
std::optional<Message> deserialize(std::span<const uint8_t> buffer) {
    ByteReader reader(buffer);

    // Read message tag
    uint8_t tag = reader.read_u8();
    if (reader.failed()) {
        return std::nullopt;
    }

    // Pack data based on tag
    switch (tag) {
        case kHandshakeTag: {
            std::optional<Handshake> handshake = read_handshake(reader);
            if (!handshake.has_value()) {
                return std::nullopt;
            }
            return Message(*handshake);
        }
        case kFileMetadataTag: {
            std::optional<FileMetadata> metadata = read_file_metadata(reader);
            if (!metadata.has_value()) {
                return std::nullopt;
            }
            return Message(*metadata);
        }
        case kFileChunkTag: {
            std::optional<FileChunk> chunk = read_file_chunk(reader);
            if (!chunk.has_value()) {
                return std::nullopt;
            }
            return Message(*chunk);
        }
        default:
            // Unknown tag byte -- likely a corrupted or malformed
            // message. Reject it rather than guessing.
            return std::nullopt;
    }
}

}
