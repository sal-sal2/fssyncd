#pragma once

#include "landrop/concurrent_queue.hpp"

#include <functional>
#include <thread>
#include <vector>

namespace landrop {

// A fixed-size thread pool that executes submitted tasks
// Tasks are callable functions with no return value
class TaskManager {
public:
    using Task = std::function<void()>;

    // Maximum number of pending tasks allowed in the queue at one time
    static constexpr size_t kDefaultQueueCapacity = 1024;

    // Creates worker threads and prepares the queue
    explicit TaskManager(size_t worker_count = std::thread::hardware_concurrency(), size_t queue_capacity = kDefaultQueueCapacity);

    // Closes the task queue, signals workers to shutdown, and waits for all workers to join
    ~TaskManager();

    // Blocks copy and move constructor and assignment
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    // Pushes a new task into the shared queue
    // If the queue is full, this will block the the calling thread until space frees up
    void submit(Task task);

private:
    // The loop every worker thread runs
    // Pops tasks from task_queue and executes them until the queue is closed and empty
    void worker_loop();

    ConcurrentQueue<Task> task_queue_;
    std::vector<std::jthread> workers_;
};

}