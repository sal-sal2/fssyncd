#pragma once

#include "include/encryptor.hpp"
#include "include/file_event.hpp"
#include "include/hasher.hpp"
#include "include/message.hpp"
#include "include/networking.hpp"
#include "include/thread_pool.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>

namespace fssyncd {

// Tracks the state for file dowload
struct IncomingTransfer {
    std::string local_path;
    uint32_t expected_chunks = 0;
    uint32_t chunks_received = 0;
    std::ofstream out_file;
};

class SyncEngine {
public:
    SyncEngine(ThreadPool& thread_pool, Networking& networking, const Encryptor& encryptor, std::string watched_directory);

    // Submits file event to thread pool to avoid blocking the caller thread
    void on_file_event(const FileEvent& event);

    // Sends message to handle process filetype
    void on_message(const Message& message);

private:
    static constexpr size_t CHUNK_SIZE_BYTES = 64 * 1024; // 64 KB

    // Outgoing file processing
    // Processes file change by verifying if content was modified, and starts file transfer 
    void process_file_change(FileEvent event);

    // Compares file hash against known hashes to check file contents actually changed
    bool content_actually_changed(const std::string& path, const Hash& current_hash);

    // Chunks, encrypts, and sends file metadata over the network
    void send_file(const std::string& path, const std::vector<uint8_t>& file_bytes);

    // Reads content of file from computer to buffer
    std::vector<uint8_t> read_file_bytes(const std::string& path);

    // Incoming file processing
    // Prepares incoming file transfer and opens output file stream for write
    void handle_file_info(const Message& message);

    // Decrypts file chunks and writes the plaintext bytes to the computer
    void handle_file_chunk(const Message& message);

    // Takes filename from absolute path
    std::string to_file_name(const std::string& local_path) const;

    // Gets file's local path within the watched directory
    std::string to_local_path(const std::string& file_name) const;

    ThreadPool& thread_pool_;
    Networking& networking_;
    const Encryptor& encryptor_;
    std::string watched_directory_;

    // Mutex to protect concurrent access to the map
    std::mutex known_hashes_mutex_;
    // Maps file paths to their last known hash
    std::unordered_map<std::string, Hash> known_hashes_;

    std::optional<IncomingTransfer> incoming_transfer_;
};
}