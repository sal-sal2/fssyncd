#include "include/sync_daemon.hpp"

#include <iostream>

namespace fssyncd {

SyncDaemon::SyncDaemon(Config config) : config_(std::move(config)), encryptor_(config.pre_shared_key) {}

SyncDaemon::~SyncDaemon() {
    stop();
}

void SyncDaemon::start() {
    if (running_) {
        return;
    }

    running_ = true;

    thread_pool_ = std::make_unique<ThreadPool>();
    networking_ = std::make_unique<Networking>(config_.tcp_port);
    networking_->connect_to_peer(config_.peer_ip, config_.peer_port);

    sync_engine_ = std::make_unique<SyncEngine>(*thread_pool_, *networking_, encryptor_, config_.watched_directory);
    networking_->on_message([this](const Message& message) {sync_engine_->on_message(message)});

    file_watcher_ = std::make_unique<FileWatcher>(config_.watched_directory, [this](const FileEvent& event) {
        sync_engine_->on_file_event(event);
    });

    std::cout << "[fssyncd] watching " << config_.watched_directory << ", listening on port " << config_.tcp_port
        << ", connecting to peer " << config_.peer_ip << ":" << config_.peer_port << "\n";
    
}

void SyncDaemon::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Stop them in reverse order
    file_watcher_.reset();
    sync_engine_.reset();
    networking_.reset();
    thread_pool_.reset();

    std::cout << "[fssyncd] stopped\n"
}


}