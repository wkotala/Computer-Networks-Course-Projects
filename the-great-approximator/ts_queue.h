#ifndef TS_QUEUE_H
#define TS_QUEUE_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

// Thread-safe queue used to store messages between threads in client_logic.

template <typename T>
class ThreadSafeQueue {
 public:
    ThreadSafeQueue() : queue(), mutex(), cond() {}

    void push(const T& item) {
        std::scoped_lock<std::mutex> lock(mutex);
        queue.push(item);
        cond.notify_one();
    }

    void push(T&& item) {
        std::scoped_lock<std::mutex> lock(mutex);
        queue.push(std::move(item));
        cond.notify_one();
    }

    // Blocks until an item is available.
    T pop() {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [this] { return !queue.empty(); });
        T item = std::move(queue.front());
        queue.pop();
        return item;
    }

    // Blocks until an item is available or timeout is reached.
    bool try_pop_for(T& out_item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        if (cond.wait_for(lock, timeout, [this] { return !queue.empty(); })) {
            out_item = std::move(queue.front());
            queue.pop();
            return true;
        }
        return false;
    }

    // Returns immediately if no item is available.
    bool try_pop(T& out_item) {
        std::unique_lock<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }
        out_item = std::move(queue.front());
        queue.pop();
        return true;
    }

 private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cond;
};

#endif // TS_QUEUE_H