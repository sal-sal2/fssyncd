#pragma once

#include "include/file_event.hpp"

#include <functional>
#include <stop_token>
#include <string>
#include <thread>

namespace fssyncd {

class FileWatcher {
public:
    using EventCallback = std::function<void(const FileEvent&)>;

    FileWatcher(std::string watched_directory, EventCallback on_event);

    ~FileWatcher();

private:
    // Periodically monitors a worker thread and see if the main thread requested shut down
    void watch_loop(std::stop_token stop_token);

    // Checks if the inotify file descriptor has data ready to be read
    bool inotify_has_data();

    // Reads one batch of inotify events and pushes a FileEvent
    void read_and_dispatch_events();

    std::string watched_directory_;
    EventCallback on_event_;

    // File descriptor (-1 unititialized)
    int inotify_fd_ = -1;

    // Watch ID from the OS (-1 unititialized)
    int watch_descriptor_ = -1;

    std::jthread worker_thread_;
};

}
