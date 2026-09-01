#pragma once

#include <functional>
#include <thread>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>

namespace fssyncd {

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Pushes a new task into the shared queue
    void submit(Task task);

private:
    // The loop every worker thread runs
    // Pops tasks from queue
    void worker_loop(std::stop_token stop_token);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Task> tasks_;

    std::vector<std::jthread> workers_;

};

}