#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace landrop {

template <typename T>
class ConcurrentQueue {
public:
    // Initialize the queue with capacity
    explicit ConcurrentQueue(size_t capacity) : capacity_(capacity) {
    }

    void push(T item) {
        std::unique_lock lock(mutex_);

        // Wait until there is room in the queue, or the queue is closed
        not_full_.wait(lock, [this] {
            return queue_.size() < capacity_ || closed_;
        });

        // Exit push if the queue was closed while waiting
        if (closed_) {
            return;
        }

        queue_.push(std::move(item));

        // Wake up one thread waiting to pop data
        not_empty_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock lock(mutex_);

        // Wait until data is in the queue, or the queue is closed
        not_empty_.wait(lock, [this] {
            return !queue_.empty() || closed_;
        });

        // Return empty if we woke up due to closure and the queue has been drained
        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();

        // Wake up one thread waiting to push data
        not_full_.notify_one();
        return item;
    }

    std::optional<T> try_pop() {
        std::lock_guard lock(mutex_);

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();

        // Notify any blocked producer that space is available
        not_full_.notify_one();
        return item;
    }

    // Shut down the queue, and unblock all waiting producers and consumers
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }

        // Wake up all producers and consumers to let them exit safely
        not_empty_.notify_all();
        not_full_.notify_all();
    }

private:
    size_t capacity_;
    std::queue<T> queue_;
    bool closed_ = false;

    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

}
