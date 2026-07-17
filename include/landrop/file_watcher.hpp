#pragma once

#include "landrop/concurrent_queue.hpp"
#include "landrop/file_event.hpp"

#include <stop_token>
#include <string>
#include <thread>

namespace landrop {

class FileWatcher {
public:
    // Initialzes inotify, registers the directory, and starts the worker thread
    FileWatcher(std::string watched_directory, ConcurrentQueue<FileEvent>& event_queue);

    // Stops the worker thread, joins it, and closes the inotify watch and file descriptor
    ~FileWatcher();

    //  Prevent Copying
    // Deletes the copy constructor and copy assignment operator to prevent duplicates
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    //  Prevent Moving
    // Deletes the move constructor and move assignment operator
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

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
