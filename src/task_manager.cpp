#include "landrop/task_manager.hpp"

#include <algorithm>
#include <iostream>

namespace fssyncd {

TaskManager::TaskManager(size_t worker_count, size_t queue_size) : task_queue_(queue_size) {
    size_t actual_worker_count = std::max<size_t>(1, worker_count);

    workers_.reserve(actual_worker_count);
    for (size_t i = 0; i < actual_worker_count; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

TaskManager::~TaskManager() {
    shutdown();
}

void TaskManager::submit(Task task) {
    task_queue_.push(std::move(task));
}

void TaskManager::shutdown() {
    std::call_once(shutdown_flag_, [this] {
        task_queue_.close(); 
        workers_.clear();
    });
}

void TaskManager::worker_loop() {
    while (auto task = task_queue_.pop()) {
        // Prevents crashing worker thread
        try {
            (*task)(); // Executre the std::function task
        } catch (const std::exception& e) {
            std::cerr << "[TaskManager] worker threw an exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[TaskManager] unknown exception\n";
        }
    }
}

}