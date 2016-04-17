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

    // it seems that you should user peek() OR pop() function only. Otherwise there concurrent error will occur
    bool peek(T &res) {
        //Log::d("okay, peek");
        std::unique_lock<std::mutex> mlock(mutex);
        //Log::d("pp1");
        if (queue.empty()) {
            //Log::d("pp2");
            return false;
        }
        Log::d("pp3");
        res = queue.front();
        Log::d("pp4");
        queue.pop();
        Log::d("pp5");
        return true;
    }

    T pop() {
        Log::d("popping");
        std::unique_lock<std::mutex> lk(mutex);
        Log::d("cp1");
        cond.wait(lk, [this] { return !queue.empty(); });
        Log::d("cp2");
        auto value(queue.front());
        Log::d("cp3");
        queue.pop();
        Log::d("cp4");
        return value;
    }

    void push(const T &item) {
        Log::d("pushing");
        std::lock_guard<std::mutex> lk(mutex);
        Log::d("xx1");
        queue.push(item);
        Log::d("xx2");
        cond.notify_one();
        Log::d("xx3");
    }

private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cond;
};

#endif //KIMBERLY_COCURRENT_QUEUE_H