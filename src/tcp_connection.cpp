#include "landrop/tcp_connection.hpp"

#include <iostream>
#include <optional>
#include <utility>

namespace landrop {

namespace {
// Helpers
// Encodes a 32 bit integer length into 4 big-endian bytes
std::array<uint8_t, 4> encode_length_header(uint32_t length) {
    return {
        static_cast<uint8_t>(length >> 24),
        static_cast<uint8_t>(length >> 16),
        static_cast<uint8_t>(length >> 8),
        static_cast<uint8_t>(length),
    };
}

// Decodes 4 big-endian bytes back into a 32-bit integer
uint32_t decode_length_header(const std::array<uint8_t, 4>& header) {
    return (static_cast<uint32_t>(header[0]) << 24) | 
           (static_cast<uint32_t>(header[1]) << 16) |
           (static_cast<uint32_t>(header[2]) << 8)  | 
            static_cast<uint32_t>(header[3]);
}

}


TcpConnection::TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

TcpConnection::~TcpConnection() {
    close();
}

void TcpConnection::send(const std::vector<uint8_t>& data, asio::error_code& ec) {
    asio::write(socket_, asio::buffer(data), ec);
}

void TcpConnection::start_receiving(ReceiveHandler handler) {
    // Moves ownership of input parameter into receive_handler_ and starts the first read operation
    receive_handler_ = std::move(handler);
    start_read();
}

void TcpConnection::send_message(const Message& message, asio::error_code& ec) {
    // Convert Message object to bytes payload using serialize from message.cpp
    std::vector<uint8_t> payload = serialize(message);

    // Create the 4-byte size header matching payload length
    std::array<uint8_t, 4> header = encode_length_header(static_cast<uint32_t>(payload.size()));

    // Combine header and payload into one buffer
    std::vector<uint8_t> framed_message;
    framed_message.reserve(header.size() + payload.size());
    framed_message.insert(framed_message.end(), header.begin(), header.end());
    framed_message.insert(framed_message.end(), payload.begin(), payload.end());

    // Write the combined bytes packet to the network
    send(framed_message, ec);
}

void TcpConnection::start_receiving_messages(MessageHandler handler) {
    message_handler_ = std::move(handler);
    start_read_header();
}

void TcpConnection::on_disconnect(DisconnectHandler handler) {
    disconnect_handler_ = std::move(handler);
}

bool TcpConnection::is_open() const {
    return socket_.is_open();
}

void TcpConnection::close() {
    if (socket_.is_open()) {
        asio::error_code ec;
        socket_.close(ec);
    }
}


void TcpConnection::start_read() {
    socket_.async_read_some(
        asio::buffer(read_buffer_),
        std::bind(&TcpConnection::on_bytes_received, this, std::placeholders::_1, std::placeholders::_2));
}


void TcpConnection::on_bytes_received(const asio::error_code& error, size_t bytes_transferred) {
    if (error) {
        return; 
    }
    std::vector<uint8_t> received_bytes(read_buffer_.begin(), read_buffer_.begin() + bytes_transferred);

    receive_handler_(received_bytes);

    start_read();
}

void TcpConnection::start_read_header() {
    asio::async_read(
        socket_,
        asio::buffer(header_buffer_),
        std::bind(&TcpConnection::on_header_received, this, std::placeholders::_1, std::placeholders::_2));
}


void TcpConnection::on_header_received(const asio::error_code& error, size_t) {
    if (error) {
        fail_framed_connection("read error while waiting for message header: " + error.message());
        return;
    }

    uint32_t body_length = decode_length_header(header_buffer_);

    if (body_length > kMaxMessageBodySize) {
        fail_framed_connection(
            "message body length " + std::to_string(body_length) + " exceeds max allowed " +
            std::to_string(kMaxMessageBodySize) + " - closing connection");
        return;
    }

    body_buffer_.resize(body_length);
    start_read_body();
}

void TcpConnection::start_read_body() {
    asio::async_read(
        socket_,
        asio::buffer(body_buffer_),
        std::bind(&TcpConnection::on_body_received, this, std::placeholders::_1, std::placeholders::_2));
}

void TcpConnection::on_body_received(const asio::error_code& error, size_t) {
    if (error) {
        fail_framed_connection("read error while waiting for message body: " + error.message());
        return;
    }

    std::optional<Message> message = deserialize(body_buffer_);
    if (!message.has_value()) {
        fail_framed_connection("failed to deserialize message body (malformed or corrupted)");
        return;
    }
    message_handler_(*message);
    start_read_header();
}

void TcpConnection::fail_framed_connection(const std::string& reason) {
    std::cerr << "[TcpConnection] " << reason << "\n";

    close();

    if (disconnect_handler_) {
        disconnect_handler_(reason);
    }
}

}