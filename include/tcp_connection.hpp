#pragma once

#include <asio.hpp>

#include "landrop/message.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fssyncd {

// Max message size 16 MB
inline constexpr uint32_t MAX_MESSAGE_BODY_SIZE = 16u * 1024u * 1024u;

class TcpConnection {
public:
    // Callback function handles a received deserialized Message
    using MessageCallBack = std::function<void(const Message&)>;

    // Callback function handles connection drop or framing failure
    using DisconnectCallBack = std::function<void(const std::string& reason, const asio::error_code& error)>;

    explicit TcpConnection(asio::ip::tcp::socket socket);

    ~TcpConnection();

    // Disable copy
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Sends bytes directly over the socket synchronously
    void send(const std::vector<uint8_t>& data, asio::error_code& ec);

    // Converts a Message object into bytes, attaches a header, and sends it
    void send_message(const Message& message, asio::error_code& ec);

    // Saves the callback function and starts the asynchronous reading loop for incoming messages of the socket
    void start_receiving_messages(MessageCallBack cb);

    // Saves the callback function that will be called if the connection fails or drops
    void on_disconnect(DisconnectCallBack cb);

    // Checks if socket is open
    bool is_open() const;

    // Safely closes the socket
    void close();

private:
    // Tell Asio to asynchronously wait for and read a 4 byte length header from the socket
    void start_read_header();

    // Runs an Asio callback when finished reading the header
    void on_header_received(const asio::error_code& error, size_t bytes_transferred);

    // Tells Asio to asynchronously wait for and read the message body from the socket
    void start_read_body();

    // Runs Asio callback when finished reading the full message body 
    void on_body_received(const asio::error_code& error, size_t bytes_transferred);

    // Helper method to handle errors by logging the errors, closing the socket, and running the disconnection callback
    void fail_framed_connection(const std::string& reason, const asio::error_code& error = {});

    asio::ip::tcp::socket socket_;

    std::array<uint8_t, 4> header_buffer_{};
    std::vector<uint8_t> body_buffer_;

    // Holds the saved callback
    MessageCallBack on_message_cb_;
    DisconnectCallBack on_disconnect_cb_;
};

}