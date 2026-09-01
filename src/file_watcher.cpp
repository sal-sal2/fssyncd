#include "include/file_watcher.hpp"

#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <climits>

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace fssyncd {

namespace {

constexpr size_t MAX_EVENTS = 16;
constexpr size_t EVENT_BUFFER_SIZE = MAX_EVENTS * (sizeof(struct inotify_event) + NAME_MAX + 1);

constexpr int TIMEOUT_MS = 200;
constexpr uint32_t WATCH_MASK = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE;

// Translate inotify flags to file event type
bool mask_to_event_type(uint32_t mask, FileEventType& out_type) {
    if (mask & IN_CREATE) {
        out_type = FileEventType::Created;
        return true;
    }
    if (mask & IN_CLOSE_WRITE) {
        out_type = FileEventType::Modified;
        return true;
    }
    if (mask & IN_DELETE) {
        out_type = FileEventType::Deleted;
        return true;
    }
    return false;
}
}

FileWatcher::FileWatcher(std::string watched_directory, EventCallback on_event) : watched_directory_(std::move(watched_directory)), on_event_(std::move(on_event)) {

    inotify_fd_ = inotify_init1(0);
    if (inotify_fd_ == -1) {
        throw std::runtime_error(std::string("FileWatcher: inotify_init1 failed: ") + std::strerror(errno));
    }

    watch_descriptor_ = inotify_add_watch(inotify_fd_, watched_directory_.c_str(), WATCH_MASK);
    if (watch_descriptor_ == -1) {
        close(inotify_fd_);
        throw std::runtime_error(std::string("FileWatcher: inotify_add_watch failed: ") + std::strerror(errno));
    }

    // Both file descriptor and watch id are valid, start the background worker loop
    worker_thread_ = std::jthread([this](std::stop_token stop_token) { watch_loop(stop_token); });
}

FileWatcher::~FileWatcher() {
    // Manually join the thread
    worker_thread_.request_stop();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    if (watch_descriptor_ != -1) {
        inotify_rm_watch(inotify_fd_, watch_descriptor_);
    }

    if (inotify_fd_ != -1) {
        close(inotify_fd_);
    }
}

void FileWatcher::watch_loop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        if (inotify_has_data()) {
            read_and_dispatch_events();
        }
    }
}

bool FileWatcher::inotify_has_data() {
    struct pollfd poll_target{};
    poll_target.fd = inotify_fd_;
    poll_target.events = POLLIN;

    int poll_result = poll(&poll_target, 1, TIMEOUT_MS);

    if (poll_result == -1) {
        if (errno != EINTR) {
            std::cerr << "[FileWatcher] poll() failed: " << std::strerror(errno) << "\n";
        }
        return false;
    }

    // Timeout, no changes
    if (poll_result == 0) {
        return false;
    }

    // Event occured
    return (poll_target.revents & POLLIN) != 0;
}

void FileWatcher::read_and_dispatch_events() {
    char buffer[EVENT_BUFFER_SIZE];

    ssize_t bytes_read = read_retry(inotify_fd_, buffer, sizeof(buffer));

    if (bytes_read == -1) {
        std::cerr << "[FileWatcher] read() failed: " << std::strerror(errno) << "\n";
        return;
    }
    if (bytes_read == 0) {
        return;
    }

    // Parse the buffer, filenames have different length
    ssize_t offset = 0;
    while (offset < bytes_read) {
        auto* event = reinterpret_cast<struct inotify_event*>(buffer + offset);

        FileEventType event_type;
        // Translate, package and push the the file event to the queue
        if (event->len > 0 && mask_to_event_type(event->mask, event_type)) {
            FileEvent packed_event{watched_directory_ + "/" + event->name, event_type};
            on_event_(packed_event);
        }

        offset += static_cast<ssize_t>(sizeof(struct inotify_event) + event->len);
    }
}

} 
