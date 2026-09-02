#pragma once

#include "include/config.hpp"
#include "include/encryptor.hpp"
#include "include/file_watcher.hpp"
#include "include/thread_pool.hpp"
#include "include/networking.hpp"
#include "include/sync_engine.hpp"

#include <memory>

namespace fssyncd {

class SyncDaemon {
public: 
    explicit SyncDaemon(Config config);
    ~SyncDaemon();

    // Initializes subsystems, connects to peers, starts message callbacks, and starts background monitoring
    void start();

    // Stops background monitoring and stops subsystems in reverse order in start()
    void stop();
private:
    Config config_;
    Encryptor encryptor_;


    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<Networking> networking_;
    std::unique_ptr<SyncEngine> sync_engine_;
    std::unique_ptr<FileWatcher> file_watcher_;

    bool running_ = false;
};
}
