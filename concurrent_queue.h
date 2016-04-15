//
// Created by jackson on 15.04.16.
//

#ifndef KIMBERLY_COCURRENT_QUEUE_H
#define KIMBERLY_COCURRENT_QUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template<typename T>
class concurrent_queue {
public:
    concurrent_queue() { }

    concurrent_queue(const concurrent_queue &) = delete;

    concurrent_queue &operator=(const concurrent_queue &) = delete;

    // it seems that you should user peek() OR pop() function only. Otherwise there concurrent error will occur
    bool peek(T &res) {
        std::unique_lock<std::mutex> mlock(mutex);
        if (queue.empty()) {
            return false;
        }
        res = queue.front();
        queue.pop();
        return true;
    }

    T pop() {
        std::unique_lock<std::mutex> mlock(mutex);
        while (queue.empty()) {
            cond.wait(mlock);
        }
        auto item = queue.front();
        queue.pop();
        return item;
    }

    void push(const T &item) {
        std::unique_lock<std::mutex> mlock(mutex);
        queue.push(item);
        mlock.unlock();
        cond.notify_one();
    }

private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cond;
};

#endif //KIMBERLY_COCURRENT_QUEUE_H