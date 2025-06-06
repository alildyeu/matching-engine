#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional> // For a non-blocking pop that returns a value

// A thread-safe queue implementation.
// Template parameter T is the type of elements stored in the queue.
template <typename T>
class ThreadSafeQueue {
public:
    // Default constructor.
    ThreadSafeQueue() = default;

    // Deleted copy constructor and copy assignment operator to prevent accidental copying,
    // as copying mutexes and condition variables is problematic.
    ThreadSafeQueue(const ThreadSafeQueue& other) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue& other) = delete;

    // Move constructor and move assignment operator can be defaulted or implemented
    // if specific move semantics for the mutex/condition_variable are needed,
    // but often, default behavior or deleting them is sufficient for simple use cases.
    // For simplicity, we'll default them, assuming the underlying members are movable
    // or that move operations are not a primary use case for this specific queue instance.
    ThreadSafeQueue(ThreadSafeQueue&& other) noexcept = default;
    ThreadSafeQueue& operator=(ThreadSafeQueue&& other) noexcept = default;

    // Pushes an item onto the queue.
    // item: The item to be added.
    void push(T item) {
        // Lock the mutex to protect shared data.
        // The lock_guard automatically releases the mutex when it goes out of scope.
        std::lock_guard<std::mutex> lock(mtx_);
        
        // Add the item to the underlying queue.
        data_queue_.push(std::move(item)); // Use std::move for efficiency if T is movable
        
        // Notify one waiting thread (if any) that an item is available.
        cond_var_.notify_one();
    }

    // Pops an item from the queue.
    // This function blocks if the queue is empty until an item becomes available.
    // Returns the item from the front of the queue.
    T pop() {
        // Acquire a unique_lock to protect shared data.
        // unique_lock is used with condition_variable as it can be unlocked and re-locked.
        std::unique_lock<std::mutex> lock(mtx_);
        
        // Wait until the queue is not empty.
        // The lambda function is a predicate that checks the condition.
        // The condition variable releases the lock while waiting and reacquires it before returning.
        cond_var_.wait(lock, [this] { return !data_queue_.empty(); });
        
        // Retrieve the item from the front of the queue.
        // std::move is used to efficiently transfer ownership if T is movable.
        T value = std::move(data_queue_.front());
        data_queue_.pop();
        
        return value;
    }

    // Tries to pop an item from the queue without blocking.
    // item: A reference to store the popped item if successful.
    // Returns true if an item was successfully popped, false if the queue was empty.
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (data_queue_.empty()) {
            return false; // Queue is empty, nothing to pop.
        }
        item = std::move(data_queue_.front());
        data_queue_.pop();
        return true;
    }

    // Tries to pop an item from the queue without blocking.
    // Returns std::optional<T> which contains the value if successful, or std::nullopt if empty.
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (data_queue_.empty()) {
            return std::nullopt; // Queue is empty
        }
        T value = std::move(data_queue_.front());
        data_queue_.pop();
        return value;
    }


    // Checks if the queue is empty.
    // Returns true if the queue is empty, false otherwise.
    bool empty() const {
        // Lock the mutex to safely access the shared queue.
        std::lock_guard<std::mutex> lock(mtx_);
        return data_queue_.empty();
    }

    // Returns the number of items in the queue.
    size_t size() const {
        // Lock the mutex to safely access the shared queue.
        std::lock_guard<std::mutex> lock(mtx_);
        return data_queue_.size();
    }

private:
    // The underlying std::queue to store elements.
    std::queue<T> data_queue_;
    
    // Mutex to protect access to the data_queue_.
    // mutable is used so that empty() and size() (const methods) can lock the mutex.
    mutable std::mutex mtx_; 
    
    // Condition variable to wait for items to be added to the queue.
    std::condition_variable cond_var_;
};

