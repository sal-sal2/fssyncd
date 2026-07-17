#include "landrop/file_watcher.hpp"

#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <climits>

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace landrop {

namespace {

// Max number of filesystem events and buffer size
constexpr size_t kMaxEventsPerRead = 16;
constexpr size_t kEventBufferSize = kMaxEventsPerRead * (sizeof(struct inotify_event) + NAME_MAX + 1);

constexpr int kPollTimeoutMs = 200;
constexpr uint32_t kWatchMask = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE;

// Helpers
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

// Read and retry on interruptions
ssize_t read_with_eintr_retry(int fd, void* buffer, size_t buffer_size) {
    while (true) {
        ssize_t bytes_read = read(fd, buffer, buffer_size);
        if (bytes_read == -1 && errno == EINTR) {
            continue;
        }
        return bytes_read;
    }
}

}

FileWatcher::FileWatcher(std::string watched_directory, ConcurrentQueue<FileEvent>& event_queue)
    : watched_directory_(std::move(watched_directory)), event_queue_(event_queue) {

    // Create inotify instance, a file descriptor that acts as the direct communication bridge (the pipeline) with the linux OS to receive filesystem notifications
    inotify_fd_ = inotify_init1(0);
    if (inotify_fd_ == -1) {
        throw std::runtime_error(
            std::string("FileWatcher: inotify_init1 failed: ") + std::strerror(errno));
    }

    // Connect the communication path to our target directory using our event filters
    watch_descriptor_ = inotify_add_watch(inotify_fd_, watched_directory_.c_str(), kWatchMask);
    if (watch_descriptor_ == -1) {
        // Clean up our communication path if the connection fails
        close(inotify_fd_);
        throw std::runtime_error(
            std::string("FileWatcher: inotify_add_watch failed for \"") + watched_directory_ +
            "\": " + std::strerror(errno));
    }

    // Both OS resources are valid, start the background worker loop
    worker_thread_ = std::jthread([this](std::stop_token stop_token) { watch_loop(stop_token); });
}

FileWatcher::~FileWatcher() {
    // Signal the thread to stop and wait for it to exit completely
    worker_thread_.request_stop();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // Unhook the directory watch subscription from the OS
    if (watch_descriptor_ != -1) {
        inotify_rm_watch(inotify_fd_, watch_descriptor_);
    }

    // Close the main communication path to the linux OS
    if (inotify_fd_ != -1) {
        close(inotify_fd_);
    }
}

void FileWatcher::watch_loop(std::stop_token stop_token) {
    // Monitor thread and shutdown requests
    while (!stop_token.stop_requested()) {
        if (inotify_fd_is_readable()) {
            read_and_dispatch_events();
        }
    }
}

bool FileWatcher::inotify_fd_is_readable() {
    // Watch the file descriptor and alert when file events arrive
    struct pollfd poll_target{};
    poll_target.fd = inotify_fd_;
    poll_target.events = POLLIN;

    int poll_result = poll(&poll_target, 1, kPollTimeoutMs);

    // Error
    if (poll_result == -1) {
        if (errno != EINTR) {
            std::cerr << "[FileWatcher] poll() failed: " << std::strerror(errno) << "\n";
        }
        return false;
    }

    // Timeout, no changes
    if (poll_result == 0) {
        // Timed out; nothing ready. Loop back around and check the stop
        // token again.
        return false;
    }

    // Event occured
    return (poll_target.revents & POLLIN) != 0;
}

void FileWatcher::read_and_dispatch_events() {
    char buffer[kEventBufferSize];

    // Read and put the waiting file events to the buffer
    ssize_t bytes_read = read_with_eintr_retry(inotify_fd_, buffer, sizeof(buffer));

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
        auto* raw_event = reinterpret_cast<struct inotify_event*>(buffer + offset);

        FileEventType event_type;
        // Translate, package and push the the file event to the queue
        if (raw_event->len > 0 && mask_to_event_type(raw_event->mask, event_type)) {
            FileEvent event{
                .path = watched_directory_ + "/" + raw_event->name,
                .type = event_type,
                .timestamp = std::chrono::system_clock::now(),
            };
            
            event_queue_.push(std::move(event));
        }

        offset += static_cast<ssize_t>(sizeof(struct inotify_event) + raw_event->len);
    }
}

} 
