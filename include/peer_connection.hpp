#pragma once

#include "include/message.hpp"

#include <asio.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>

namespace fssyncd {

// Max message size 16 MB
inline constexpr uint32_t MAX_MESSAGE_BODY_SIZE = 16u * 1024u * 1024u;

class PeerConnection {
public:
    // Callback function handles a received deserialized Message
    using MessageCallBack = std::function<void(const Message&)>;

    // Callback function handles connection drop or framing failure
    using DisconnectCallBack = std::function<void(const std::string& reason)>;

    explicit PeerConnection(asio::ip::tcp::socket socket);
    ~TcpConnection();

    // Saves callbacks and starts the async read loop for incoming messages
    void start_receiving(MessageCallBack on_message, DisconnectCallBack on_disconnect);

    // Serializes, frames, and sends a message over the socket synchronously
    bool send(const Message& message);

    bool is_open() const;
    void close();

private:
    // Reads a 4 byte length header from the socket
    void read_header();

    // Runs a callback when finished reading the header
    void on_header_read(const asio::error_code& error);

    // Asynchronously reads the message body from the socket
    void read_body(uint32_t body_length);

    // Runs a callback when finished reading the full message body 
    void on_body_read(const asio::error_code& error);

    // Sends the error message, closes the socket, and calls the disconnect callback
    void fail(const std::string& reason);

    asio::ip::tcp::socket socket_;
    std::mutex write_mutex_;

    std::array<uint8_t, 4> header_buffer_{};
    std::vector<uint8_t> body_buffer_;

    MessageCallBack on_message_cb_;
    DisconnectCallBack on_disconnect_cb_;
};

}