#pragma once

#include "landrop/concurrent_queue.hpp"
#include "landrop/file_event.hpp"

#include <stop_token>
#include <string>
#include <thread>

namespace fssyncd {

class FileWatcher {
public:
    FileWatcher(std::string watched_directory, ConcurrentQueue<FileEvent>& event_queue);

    ~FileWatcher();

private:
    // Periodically monitors a worker thread and see if the main thread requested shut down
    void watch_loop(std::stop_token stop_token);

    // Checks if the inotify file descriptor has data ready to be read
    bool inotify_fd_is_readable();

    // Reads one batch of inotify events and pushes a FileEvent for each relevant one onto event_queue_.
    void read_and_dispatch_events();

    // Watched directory
    std::string watched_directory_;

    // File descriptor (-1 unititialized)
    int inotify_fd_ = -1;

    // Watch ID from the OS (-1 unititialized)
    int watch_descriptor_ = -1;

    ConcurrentQueue<FileEvent>& event_queue_;

    std::jthread worker_thread_;
};

}
