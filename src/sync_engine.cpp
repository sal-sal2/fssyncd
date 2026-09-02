#include "include/sync_engine.hpp"

#include <iostream>
#include <algorithm>
#include <filesystem>

namespace fssyncd {

SyncEngine::SyncEngine(ThreadPool& thread_pool, Networking& networking, const Encryptor& encryptor, std::string watched_directory) :
    thread_pool_(thread_pool),
    networking_(networking),
    encryptor_(encryptor),
    watched_directory_(std::move(watched_directory)) {}

std::string SyncEngine::to_file_name(const std::string& local_path) const {
    return std::filesystem::path(local_path).filename().string();
}

std::string SyncEngine::to_local_path(const std::string& file_name) const {
    return (std::filesystem::path(watched_directory_) / file_name).string();
}

// Outgoing
void SyncEngine::on_file_event(const FileEvent& event) {
    thread_pool_.submit([this, event] { process_file_change(event); });
}

void SyncEngine::process_file_change(FileEvent event) {
    if (event.type == FileEventType::Deleted) {

        // Lock mutex while removing deleted file entry from map
        std::lock_guard<std::mutex> lock(known_hashes_mutex_);
        known_hashes_.erase(event.path);
        return;
    }

    if (event.type == FileEventType::Created) {
        return;
    }

    std::vector<uint8_t> file_bytes;
    try {
        file_bytes = read_file_bytes(event.path);
    } catch (const std::exception& error) {
        std::cerr << "[SyncEngine] failed to read " << event.path << ": " << error.what() << "\n";
        return;
    }

    Hash current_hash = hash_bytes(file_bytes);
    if (!content_actually_changed(event.path, current_hash)) {
        return;
    }

    send_file(event.path, file_bytes);
}

bool SyncEngine::content_actually_changed(const std::string& path, const Hash& current_hash) {
    // Lock to protect map from concurrent access across worker threads
    std::lock_guard<std::mutex> lock(known_hashes_mutex_);

    auto existing = known_hashes_.find(path);
    bool changed = (existing == known_hashes_.end()) || (existing->second != current_hash);
    known_hashes_[path] = current_hash;
    return changed;
}

void SyncEngine::send_file(const std::string& path, const std::vector<uint8_t>& file_bytes) {
    uint32_t chunk_count = static_cast<uint32_t>((file_bytes.size() + CHUNK_SIZE_BYTES - 1) / CHUNK_SIZE_BYTES);
    chunk_count = std::max<uint32_t>(chunk_count, 1);

    Message info;
    info.type = MessageType::FileMetadata;
    info.file_name = to_file_name(path);
    info.file_size = file_bytes.size();
    info.chunk_count = chunk_count;

    if (!networking_.send(info)) {
        std::cerr << "[SyncEngine] failed to send file info for " << path << ", stopping transfer\n";
        return;
    }

    for (uint32_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        size_t offset = static_cast<size_t>(chunk_index) * CHUNK_SIZE_BYTES;
        size_t length = std::min(CHUNK_SIZE_BYTES, file_bytes.size() - offset);

        std::vector<uint8_t> plaintext_chunk(file_bytes.begin() + offset, file_bytes.begin() + offset + length);
        
        Message chunk;
        chunk.type = MessageType::FileChunk;
        chunk.chunk_index = chunk_index;
        chunk.chunk_data = encryptor_.encrypt(plaintext_chunk);

        if (!networking_.send(chunk)) {
            std::cerr << "[SyncEngine] failed to send chunk " << chunk_index << " of " << path << ", stopping transfer\n";
            return;
        }
    }
}

std::vector<uint8_t> SyncEngine::read_file_bytes(const std::string& path) {
    // Opens file stream positioned at end to determine byte size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("failed to open file: " + path);
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(file_size));

    // Reads bytes form computer into buffer
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    return buffer;
}

void SyncEngine::on_message(const Message& message) {
    if (message.type == MessageType::FileMetadata) {
        handle_file_info(message);
    } else if (message.type == MessageType::FileChunk) {
        handle_file_chunk(message);
    }
}

void SyncEngine::handle_file_info(const Message& message) {
    std::string local_path = to_local_path(message.file_name);

    IncomingTransfer transfer;
    transfer.local_path = local_path;
    transfer.expected_chunks = message.chunk_count;

    // Creates or truncates file on computer to prepare for receiving incoming bytes
    transfer.out_file.open(local_path, std::ios::binary | std::ios::trunc);
    if (!transfer.out_file) {
        std::cerr << "[SyncEngine] failed to open " << local_path << " for incoming transfer\n";
        return;
    }

    incoming_transfer_ = std::move(transfer);
}

void SyncEngine::handle_file_chunk(const Message& message) {
    if (!incoming_transfer_.has_value()) {
        std::cerr << "[SyncEngine] got a file chunk with no transfer in progress, dropping\n";
        return;
    }

    IncomingTransfer& transfer = *incoming_transfer_;

    if (message.chunk_index != transfer.chunks_received) {
        std::cerr << "[SyncEngine] chunk arrived out of order for " << transfer.local_path << " (expected " <<
            transfer.chunks_received << ", got instead " << message.chunk_index << ") stopping transfer\n";
        incoming_transfer_.reset();
        return;
    }

    std::optional<std::vector<uint8_t>> plaintext = encryptor_.decrypt(message.chunk_data);
    if (!plaintext.has_value()) {
        std::cerr << "[SyncEngine] chunk " << message.chunk_index << " for " << transfer.local_path
            << " failed decryption. stopping transfer\n";
        incoming_transfer_.reset();
        return;
    }

    // Flushes decrypted chunk bytes directly to computer output stream
    transfer.out_file.write(reinterpret_cast<const char*>(plaintext->data()),
        static_cast<std::streamsize>(plaintext->size()));
    transfer.chunks_received++;

    if (transfer.chunks_received < transfer.expected_chunks) {
        return;
    }

    std::string completed_path = transfer.local_path;

    // Close output file stream before finishing write
    transfer.out_file.close();
    incoming_transfer_.reset();

    try {
        std::vector<uint8_t> written_bytes = read_file_bytes(completed_path);
        content_actually_changed(completed_path, hash_bytes(written_bytes));
    } catch (const std::exception& error) {
        std::cerr << "[SyncEngine] failed to re-hash " << completed_path << " after receiving it: "
            << error.what() << "\n";
    }
}
}