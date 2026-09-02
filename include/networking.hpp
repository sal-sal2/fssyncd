#pragma once

#include "include/message.hpp"
#include "include/peer_connection.hpp"

#include <asio.hpp>
#include <cstdint>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include <string>

namespace fssyncd {

inline constexpr uint32_t PROTOCOL_VERSION = 1;

class Networking {
public:
    using MessageCallback = std::function<void(const Message&)>;
    using DisconnectCallback = std::function<void()>;

    // Sets listening port, starts acceptor, starts io thread
    explicit Networking(uint16_t listen_port);

    // Closes connections, event loop, and thread
    ~Networking();

    // Starts an async tcp connection to the given IP and port
    void connect_to_peer(const std::string& peer_ip, uint16_t peer_port);

    void on_message(MessageCallback on_message);
    void on_disconnect(DisconnectCallback on_disconnect);

    // Sends a message to the peer if a valid connection and handshake happened
    bool send(const Message& message);

    // Checks both if connection and handshake complete
    bool is_connected() const;

private:
    // Listens for incoming connection requests asynchronously in a loop
    void start_accept();

    // Checks connection, performs handshake, and starts listening for incoming messages
    void handle_new_socket(asio::ip::tcp::socket socket);

    // Checks handshake and sends message to callback
    void on_message_received(const Message& message);

    // Cleans up connection and calls disconnect callback
    void on_connection_lost();

    // Runs the Asio event loop
    void run_io_context();

    asio::io_context io_context_;

    // Prevents the event loop from exiting early when no work is pending
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::ip::tcp::acceptor acceptor_;

    std::jthread io_thread_;

    mutable std::mutex connection_mutex_;
    std::unique_ptr<PeerConnection> connection_;
    bool handshake_done_ = false;

    MessageCallback on_message_cb_;
    DisconnectCallback on_disconnect_cb_;


};
}