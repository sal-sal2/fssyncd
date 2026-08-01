#pragma once

#include <asio.hpp>

#include "landrop/message.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace landrop {

// 16 MB
inline constexpr uint32_t kMaxMessageBodySize = 16u * 1024u * 1024u;

// Wrapper class around a socket to manage the life of a connection
class TcpConnection {
public:
    // Handler called when bytes/data is received each time by the peer
    using ReceiveHandler = std::function<void(const std::vector<uint8_t>&)>;

    // Handler called when a fully decoded Message has been received
    using MessageHandler = std::function<void(const Message&)>;

    // Handler called when the connection stops due to socket error or oversized length header
    using DisconnectHandler = std::function<void(const std::string& reason)>;

    // Takes ownership of a connected socket
    explicit TcpConnection(asio::ip::tcp::socket socket);

    // Closes the socket
    ~TcpConnection();

    // Prevent copying sockets
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Allow Moving
    TcpConnection(TcpConnection&&) noexcept = default;
    TcpConnection& operator=(TcpConnection&&) noexcept = default;

    // Sends data/bytes over the network synchronously, uses error code on failure
    void send(const std::vector<uint8_t>& data, asio::error_code& ec);

    // Starts a loop that gives incoming bytes to a callback function
    void start_receiving(ReceiveHandler handler);

    // Converts a Message object into bytes, attaches a 4 byte size header, and sends it
    void send_message(const Message& message, asio::error_code& ec);

    // Starts a loop that parses incoming network bytes into complete Message objects
    void start_receiving_messages(MessageHandler handler);

    // Creates a handler if message loop disconnects
    void on_disconnect(DisconnectHandler handler);

    // Checks if the socket is open
    bool is_open() const;

    // Closes the socket if it's open
    void close();

private:
    // Async

    // Starts reading available bytes from the socket
    void start_read();

    // Processes bytes after reading and starts start_read() again to keep listening
    void on_bytes_received(const asio::error_code& error, size_t bytes_transferred);

    // Reads 4 bytes and stores into header_buffer_
    void start_read_header();

    // Parses the message length and resizes body_buffer_
    void on_header_received(const asio::error_code& error, size_t bytes_transferred);

    // Reads the exact number of bytes from the header
    void start_read_body();

    // Runs after all body bytes arrive, turns bytes into a Message object and restarts the loop
    void on_body_received(const asio::error_code& error, size_t bytes_transferred);

    // Closes the socket and notifies the caller if a message loop error occurs
    void fail_framed_connection(const std::string& reason);

    asio::ip::tcp::socket socket_;
    std::array<uint8_t, 4096> read_buffer_{};
    ReceiveHandler receive_handler_;

    std::array<uint8_t, 4> header_buffer_{};
    std::vector<uint8_t> body_buffer_;
    MessageHandler message_handler_;
    DisconnectHandler disconnect_handler_;
};

}
