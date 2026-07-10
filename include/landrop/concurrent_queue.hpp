#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace landrop {

template <typename T>
class ConcurrentQueue {
public:
    explicit ConcurrentQueue(size_t capacity) : capacity_(capacity) {

    }

    //
    void push(T item) {
        std::unique_lock lock(mutex_);

        not_full_.wait(lock, [this] {
            return queue_.size() < capacity_ || closed_;
        });

        if (closed_) {
            return;
        }

        queue_.push(std::move(item));
        not_empty_.notify_one();
    }

    //
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);

        not_empty_.wait(lock, [this] {
            return !queue_.empty() || closed_;
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return item;
    }

    //
    std::optional<T> try_pop() {
        std::lock_guard lock(mutex_);

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return item;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
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
