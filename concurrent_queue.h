//
// Created by jackson on 15.04.16.
//

#ifndef KIMBERLY_COCURRENT_QUEUE_H
#define KIMBERLY_COCURRENT_QUEUE_H

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
        return false;
        Log::d("okay, peek");
        std::unique_lock<std::mutex> mlock(mutex);
        Log::d("pp1");
        if (queue.empty()) {
            Log::d("pp2");
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
        Log::d("okay, popping");
        std::unique_lock<std::mutex> mlock(mutex);
        Log::d("cp1");
        while (queue.empty()) {
            Log::d("cp2");
            cond.wait(mlock);
            Log::d("cp3");
        }
        Log::d("cp4");
        auto item = queue.front();
        Log::d("cp5");
        queue.pop();
        Log::d("cp6, " + inttostr((int) item));
        return item;
    }

    void push(const T &item) {
        Log::d("pushing");
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