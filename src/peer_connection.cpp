#include "include/peer_connection.hpp"

#include <iostream>

namespace fssyncd {

namespace {

std::array<uint8_t, 4> encode_length(uint32_t length) {
    return {
        static_cast<uint8_t>(length >> 24),
        static_cast<uint8_t>(length >> 16),
        static_cast<uint8_t>(length >> 8),
        static_cast<uint8_t>(length),
    };
}

uint32_t decode_length(const std::array<uint8_t, 4>& header) {
    return static_cast<uint32_t>(header[0] << 24) | static_cast<uint32_t>(header[1] << 16) |
        static_cast<uint32_t>(header[2] << 8) | static_cast<uint32_t>(header[3]);
}
}

PeerConnection::PeerConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

PeerConnection::~PeerConnection() {
    close();
}

bool PeerConnection::send(const Message& message) {
    std::vector<uint8_t> body = serialize_message(message);
    std::array<uint8_t, 4> header = encode_length(static_cast<uint32_t>(body.size()));

    std::vector<uint8_t> framed_message;
    framed_message.reserve(header.size() + body.size());

    framed_message.insert(framed_message.end(), header.begin(), header.end());
    framed_message.insert(framed_message.end(), body.begin(), body.end());

    std::lock_guard<std::mutex> lock(write_mutex_);
    asio::error_code ec;
    asio::write(socket_, asio::buffer(framed_message), ec);

    if (ec) {
        std::cerr << "[PeerConnection] send failed: " << ec.message() << "\n";
        return false; // mutex is unlocked
    }

    // mutex is unlocked
    return true;
}

void PeerConnection::start_receiving(essageCallBack on_message, DisconnectCallBack on_disconnect) {
    on_message_cb_ = std::move(on_message);
    on_disconnect_cb_ = std::move(on_disconnect)
    read_header();
}

void PeerConnection::on_disconnect(DisconnectCallBack cb) {
    on_disconnect_cb_ = std::move(cb);
}

bool PeerConnection::is_open() const {
    return socket_.is_open();
}

void PeerConnection::close() {
    if (socket_.is_open()) {
        asio::error_code ec;
        socket_.close(ec);
    }
}


void PeerConnection::read_header() {
    asio::async_read(socket_, asio::buffer(header_buffer_), [this](const asio::error_code& error, size_t /*bytes*/) {
        on_header_read(error);
    });
}

void PeerConnection::on_header_read(const asio::error_code& error) {
    if (error) {
        fail("error while waiting for message: " + error.message());
        return;
    }

    uint32_t body_length = decode_length(header_buffer_);

    // Reject messages larger than 16 MB
    if (body_length > MAX_MESSAGE_BODY_SIZE) {
        fail("peer sent a message larger than the maxed allowed size, closing connection");
        return;
    }

    read_body(body_length);
}

void PeerConnection::read_body(uint32_t body_length) {
    body_buffer_.resize(body_length);

    asio::async_read(socket_, asio::buffer(body_buffer_), [this](const asio::error_code& error, size_t /*bytes*/){
        on_body_read(error);
    })
}

void PeerConnection::on_body_read(const asio::error_code& error) {
    if (error) {
        fail("read error while waiting for message body: " + error.message());
        return;
    }

    // Pass the Message object to the callback
    if (on_message_cb_) {
        on_message_cb_(*message);
    }

    // Immediately listen for the next message's header
    if (socket_.is_open()) {
        read_header();
    }
}

void PeerConnection::fail_framed_connection(const std::string& reason) {
    std::cerr << "[PeerConnection] " << reason << "\n";

    close();

    if (on_disconnect_cb_) {
        on_disconnect_cb_(reason);
    }
}

}