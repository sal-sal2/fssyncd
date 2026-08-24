#include "landrop/tcp_connection.hpp"

#include <iostream>
#include <optional>
#include <utility>

namespace fssyncd {

namespace {

std::array<uint8_t, 4> encode_length_header(uint32_t length) {
    return {
        static_cast<uint8_t>(length >> 24),
        static_cast<uint8_t>(length >> 16),
        static_cast<uint8_t>(length >> 8),
        static_cast<uint8_t>(length),
    };
}

uint32_t decode_length_header(const std::array<uint8_t, 4>& header) {
    return static_cast<uint32_t>(header[0] << 24) | static_cast<uint32_t>(header[1] << 16) |
        static_cast<uint32_t>(header[2] << 8) | static_cast<uint32_t>(header[3]);
}
}

TcpConnection::TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

TcpConnection::~TcpConnection() {
    close();
}

void TcpConnection::send(const std::vector<uint8_t>& data, asio::error_code& ec) {
    asio::write(socket_, asio::buffer(data), ec);
}

void TcpConnection::send_message(const Message& message, asio::error_code& ec) {
    std::vector<uint8_t> payload = serialize(message);
    
    std::array<uint8_t, 4> header = encode_length_header(static_cast<uint32_t>(payload.size()));
    
    std::vector<uint8_t> framed_message;
    framed_message.reserve(header.size() + payload.size());

    framed_message.insert(framed_message.end(), header.begin(), header.end());
    framed_message.insert(framed_message.end(), payload.begin(), payload.end());

    // Write the complete message to the socket
    send(framed_message, ec);
}

void TcpConnection::start_receiving_messages(MessageCallBack cb) {
    on_message_cb_ = std::move(cb);
    start_read_header();
}

void TcpConnection::on_disconnect(DisconnectCallBack cb) {
    on_disconnect_cb_ = std::move(cb);
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


void TcpConnection::start_read_header() {
    // asio::async_read puts the io thread to sleep until bytes arrive
    // Fills header_buffer_ with the header
    // Calls the lambda function when reading finshes or error occurs
    asio::async_read(socket_, asio::buffer(header_buffer_), [this](const asio::error_code& error, size_t bytes_transferred) {
        on_header_received(error, bytes_transferred);
    });
}

void TcpConnection::on_header_received(const asio::error_code& error, size_t bytes_transferred) {
    if (error) {
        fail_framed_connection("read error while waiting for message header: " + error.message(), error);
        return;
    }

    uint32_t body_length = decode_length_header(header_buffer_);

    // Reject messages larger than 16 MB
    if (body_length > MAX_MESSAGE_BODY_SIZE) {
        fail_framed_connection("message body length " + std::to_string(body_length) + " exceeds max allowed " + 
        std::to_string(MAX_MESSAGE_BODY_SIZE) + " - closing connection");
        return;
    }

    // Resize the body buffer to hold the exact # bytes, then wait for the message body 
    body_buffer_.resize(body_length);
    start_read_body();
}

void TcpConnection::start_read_body() {
    // Fills the body buffer with incoming data then calls the callback or when error occurs
    asio::async_read(socket_, asio::buffer(body_buffer_), [this](const asio::error_code& error, size_t bytes_transferred) {
        on_body_received(error, bytes_transferred);
    });
}

void TcpConnection::on_body_received(const asio::error_code& error, size_t bytes_transferred) {
    if (error) {
        fail_framed_connection("read error while waiting for message body: " + error.message(), error);
        return;
    }

    std::optional<Message> message = deserialize(body_buffer_);
    if (!message.has_value()) {
        fail_framed_connection("failed to deserialize message body (malformed or corrupted)");
        return;
    }

    // Pass the Message object to the callback
    on_message_cb_(*message);

    // Immediately listen for the next message's header
    if (socket_.is_open()) {
        start_read_header();
    }
}

void TcpConnection::fail_framed_connection(const std::string& reason, const asio::error_code& error) {
    std::cerr << "[TcpConnection] " << reason << "\n";

    close();

    if (on_disconnect_cb_) {
        on_disconnect_cb_(reason, error);
    }
}

}