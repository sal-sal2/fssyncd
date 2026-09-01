#include "include/thread_pool.hpp"

#include <algorithm>
#include <iostream>

namespace fssyncd {

ThreadPool::ThreadPool(size_t thread_count) {
    // Default to 1 thread otherwise
    thread_count = std::max<size_t>(1, thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this](std::stop_token stop_token) { worker_loop(stop_token); });
    }
}

ThreadPool::~ThreadPool() {}

void ThreadPool::submit(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));   
    }
    // mutex_ is now unlocked
    cv_.notify_one();
}

void ThreadPool::worker_loop(std::stop_token stop_token) {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);

            // Blocks, waits, and releases mutex until either queue is not empty or a stop request is signaled by the stop_token
            bool has_task = cv_.wait(lock, stop_token, [this] { return !tasks_.empty(); });
            
            // Both stop_token requested and queue is empty
            if (!has_task) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        // mutex_ is now unlocked
        
        try {
            task(); // Execute the task
        } catch (const std::exception& e) {
            std::cerr << "[TaskManager] task threw an exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[TaskManager] unknown exception\n";
        }
    }
}

}