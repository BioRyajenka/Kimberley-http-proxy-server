//
// Created by jackson on 15.04.16.
//

#ifndef KIMBERLY_COCURRENT_QUEUE_H
#define KIMBERLY_COCURRENT_QUEUE_H


#include <memory>
#include <chrono>
#include <cassert>
#include <iostream>

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "util.h"

template<typename T>
class concurrent_queue {
public:
    concurrent_queue() { }

    concurrent_queue(const concurrent_queue &) = delete;

    concurrent_queue &operator=(const concurrent_queue &) = delete;

    bool peek(T &res) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }
        Log::d("pp3");
        res = queue.front();
        Log::d("pp4");
        queue.pop();
        Log::d("pp5");
        return true;
    }

    //TODO: move
    bool pop(T &res) {
        Log::d("popping");
        std::unique_lock<std::mutex> lock(mutex);
        if (releasing) {
            return false;
        }
        Log::d("cp1");
        cond.wait(lock, [this] { return !queue.empty(); });
        Log::d("cp2");
        res = queue.front();
        Log::d("cp3");
        queue.pop();
        Log::d("cp4");
        return true;
    }

    //TODO: moving instead of copying
    void push(const T &item) {
        std::lock_guard<std::mutex> lock(mutex);
        Log::d("xx1");
        queue.push(item);
        Log::d("xx2");
        cond.notify_one();
        Log::d("xx3");
    }

    void release_waiters() {
        std::lock_guard<std::mutex> lock(mutex);
        releasing = true;
        cond.notify_all();
    }

    bool is_releasing() {
        return releasing;
    }

private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cond;

    bool releasing = false;
};

#endif //KIMBERLY_COCURRENT_QUEUE_H