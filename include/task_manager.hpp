#pragma once

#include "landrop/concurrent_queue.hpp"

#include <functional>
#include <thread>
#include <vector>

namespace fssyncd {

// Thread pool for background sync tasks
class TaskManager {
public:
    using Task = std::function<void()>;

    static constexpr size_t DEFAULT_QUEUE_SIZE = 1024;

    explicit TaskManager(size_t worker_count = std::thread::hardware_concurrency(), size_t queue_size = DEFAULT_QUEUE_SIZE);
    ~TaskManager();

    // Pushes a new task into the shared queue
    // If the queue is full, it blocks the the calling thread until space frees up
    void submit(Task task);

    // Stops all threads and closes the queue
    void shutdown();

private:
    // The loop every worker thread runs
    // Pops tasks from task_queue and executes them until the queue is closed and empty
    void worker_loop();

    ConcurrentQueue<Task> task_queue_;
    std::vector<std::jthread> workers_;

    std::once_flag shutdown_flag_;
};

}