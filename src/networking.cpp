#include "include/networking.hpp"

#include <iostream>


namespace fssyncd {

Networking::Networking(uint16_t listen_port) :
    work_guard_(asio::make_work_guard(io_context_)),
    acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), listen_port)) {

        start_accept();
        io_thread_ = std::jthread([this] {run_io_context(); });
}

Networking::~Networking() {
    {
        // Protect the connection while we are destroying it
        std::lock_guard<std::mutex> lock(connection_mutex_);
        if (connection_) {
            connection_->close();
        }
    }

    work_guard_.reset();
    io_context_.stop();

    //io_thread automatically joined
}

void Networking::run_io_context() {
    io_context_.run();
}

void Networking::start_accept() {
    acceptor_.async_accept([this] (const asio::error_code& error, asio::ip::tcp::socket socket) {
        if (!error) {
            handle_new_socket(std::move(socket));
        }

        start_accept();
    });
}

void Networking::connect_to_peer(const std::string& peer_ip, uint16_t peer_port) {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    asio::ip::tcp::endpoint peer_endpoint(asio::ip::make_address(peer_ip), peer_port);

    socket->async_connect(peer_endpoint, [this, socket](const asio::error_code& error) {
        if (error) {
            std::cerr << "[Networking] connect to peer failed: " << error.message() << "\n";
            return;
        }
        handle_new_socket(std::move(*socket));
    });
}

void Networking::handle_new_socket(asio::ip::tcp::socket socket) {
    // Setup lock when setting up the connection
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (connection_) {
        return;
    }

    auto new_connection = std::make_unique<PeerConnection>(std::move(socket));

    Message handshake;
    handshake.type = MessageType::Handshake;
    handshake.protocol_version = PROTOCOL_VERSION;
    if (!new_connection->send(handshake)) {
        return;
    }

    connection_ = std::move(new_connection);
    handshake_done_ = false;

    connection_->start_receiving([this](const Message& message) { on_message_received(message); }, [this](const std::string& /*reason*/) { on_connection_lost(); });    
}

void Networking::on_message_received(const Message& message) {
    if (!handshake_done_) {
        if (message.type != MessageType::Handshake || message.protocol_version != PROTOCOL_VERSION) {
            std::cerr << "[Networking] peer's handhsake didn't match, closing connectionn\n";
            
            // Setup lock on connection to protect connection on shutdown
            std::lock_guard<std::mutex> lock(connection_mutex_);
            if (connection_) {
                connection_->close();
            }
            return;
        }
        
        handshake_done_ = true;
        return;
    }

    if (on_message_cb_) {
        on_message_cb_(message);
    }
}

void Networking::on_connection_lost() {
    {
        // Lock connection on disconnect
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connection_.reset();
        handshake_done_ = false;

    }
    if (on_disconnect_cb_) {
        on_disconnect_cb_();
    }
}

void Networking::on_message(MessageCallback on_message) {
    on_message_cb_ = std::move(on_message);
}

void Networking::on_disconnect(DisconnectCallback on_disconnect) {
    on_disconnect_cb_ = std::move(on_disconnect);
}

bool Networking::send(const Message& message) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (!connection_ || !handshake_done_) {
        return false;
    }

    return connection_->send(message);
}

bool Networking::is_connected() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return connection_ != nullptr && connection_->is_open() && handshake_done_;
}
}