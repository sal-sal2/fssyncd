#include "landrop/task_manager.hpp"

#include <algorithm>
#include <iostream>

namespace landrop {

TaskManager::TaskManager(size_t worker_count, size_t queue_capacity) : task_queue_(queue_capacity) {
    // worker_count is set to hardware_concurrency, otherwise we guarantee 1 worker thread
    size_t actual_worker_count = std::max<size_t>(1, worker_count);

    workers_.reserve(actual_worker_count);
    for (size_t i = 0; i < actual_worker_count; ++i) {
        // Construct a new jthread along with 'this' to execute private functions
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
    std::call_once(shutdown_flag_, [this] {shutdown_once(); });
}

void TaskManager::shutdown_once() {
    // Closes the queue preventing new tasks and pushes
    // Existing queued tasks are executed before workers exit
    task_queue_.close();

    // Clears vector of jthreads, calls the jthread destructor, and joins all threads
    workers_.clear();
}

void TaskManager::worker_loop() {
    // Returns a valid optional<Task> when work is ready, and nullopt when queue is full to stop the loop
    while (auto task = task_queue_.pop()) {
        // Prevents crashing worker thread
        try {
            (*task)(); // Executre the std::function task
        } catch (const std::exception& e) {
            std::cerr << "[TaskManager] task threw an exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[TaskManager] task threw a non-standard exception\n";
        }
    }
}

}