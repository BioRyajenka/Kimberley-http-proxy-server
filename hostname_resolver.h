//
// Created by jackson on 29.04.16.
//

#ifndef KIMBERLY_HOSTNAME_RESOLVER_H
#define KIMBERLY_HOSTNAME_RESOLVER_H

#include "concurrent_queue.h"
#include "my_addrinfo.h"
#include <map>
#include <string.h>

class hostname_resolver {
public:
    ~hostname_resolver() {
        Log::d("releasing waiters");
        queue.release_waiters();

        for (auto &w : workers) {
            w->join();
        }
    }

    hostname_resolver(int threads) {
        for (int i = 0; i < threads; i++) {
            workers.push_back(std::make_unique<worker>(this));
        }
    }

    void add_task(std::string hostname, std::function<void(std::shared_ptr<my_addrinfo>)> task, std::function<void()> on_fail) {
        queue.push(resolver_task(hostname, task, on_fail));
    }

private:
    struct resolver_task {
        resolver_task() { }

        resolver_task(std::string hostname, std::function<void(std::shared_ptr<my_addrinfo>)> task, std::function<void()> on_fail)
                : hostname(hostname), task(task), on_fail(on_fail) { }

        std::string hostname;
        std::function<void(std::shared_ptr<my_addrinfo>)> task;
        std::function<void()> on_fail;
    };

    class worker {
        //worker(const worker &) = delete;
        //worker &operator=(const worker &) = delete;

        hostname_resolver *resolver;
        std::unique_ptr<std::thread> thread;

    public:

        worker(hostname_resolver *_resolver) : resolver(_resolver) {
            thread = std::make_unique<std::thread>((std::function<void()>) ([this] {
                Log::d("RESOLVER: resolver thread id: " + inttostr((int) pthread_self()));
                while (1) {
                    resolver_task task;
                    if (!resolver->queue.pop(task)) {
                        return;
                    }
                    std::shared_ptr<my_addrinfo> addrinfo;
                    if (resolve_hostname(task.hostname, addrinfo)) {
                        task.task(addrinfo);
                    } else {
                        task.on_fail();
                    }
                    //delete addrinfo;
                }
            }));
        }

        ~worker() {
            if (!resolver->queue.is_releasing()) {
                Log::fatal("RESOLVER: hostname_resolve_queue should be released first");
            }
        }

        void join() {
            thread->join();
        }

        bool resolve_hostname(std::string hostname, std::shared_ptr<my_addrinfo> &result) {
            Log::d("RESOLVER: \tResolving hostname \"" + hostname + "\"");
            uint16_t port = 80;

            ulong pos = hostname.find(":");
            std::string new_hostname = hostname.substr(0, pos);
            if (pos != std::string::npos) {
                port = (uint16_t) strtoint(hostname.substr(pos + 1));
            }

            Log::d("RESOLVER: \thostname is " + new_hostname + ", port is " + inttostr(port));

            auto it = resolver->cashed_hostnames.find(hostname);
            if (it != resolver->cashed_hostnames.end()) {
                Log::d("RESOLVER: taking " + new_hostname + " from cash");
                result = it->second;
                return true;
            }

            Log::d("RESOLVER: " + new_hostname + " wasn't found in cash");
            struct addrinfo hints;
            struct addrinfo *_servinfo;
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
            hints.ai_socktype = SOCK_STREAM;

            if (getaddrinfo(new_hostname.c_str(), inttostr(port).c_str(), &hints, &_servinfo) != 0) {
                Log::e("RESOLVER: getaddrinfo error");
                perror("getaddrinfo");
                return false;
            }

            std::unique_lock<std::mutex> lock(resolver->cashing_mutex);
            it = resolver->cashed_hostnames.find(hostname);
            if (it == resolver->cashed_hostnames.end()) {
                Log::d("RESOLVER: ok, not found in cash");
                result = std::make_shared<my_addrinfo>(_servinfo);
                resolver->cashed_hostnames.insert(std::make_pair(hostname, result));
            } else {
                Log::d("RESOLVER: suddenly found in cash");
                freeaddrinfo(_servinfo);
                result = it->second;
            }
            Log::d(std::string("RESOLVER: result->ptr is ") + (result->get_info() == nullptr ? "nullptr?!" : "not nullptr"));
            //*result = new my_addrinfo(_servinfo);
            return true;
        }
    };

    concurrent_queue<resolver_task> queue;

    std::map<std::string, std::shared_ptr<my_addrinfo>> cashed_hostnames;
    std::mutex cashing_mutex;

    //just for cleaning memory up
    std::vector<std::unique_ptr<worker>> workers;
};

#endif //KIMBERLY_HOSTNAME_RESOLVER_H