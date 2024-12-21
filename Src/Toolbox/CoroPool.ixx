//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <iostream>
#include <list>
#include <utility>     // for std::exchange()
#include <functional>  // for std::function
#include <coroutine>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

export module Toolbox:CoroPool;

using namespace std;

namespace netcoap {
    namespace toolbox {

        export class CoroPool;

        export class [[nodiscard]] CoroPoolTask
        {
            friend class CoroPool;
        public:
            struct promise_type;
            using CoroHdl = std::coroutine_handle<promise_type>;
        private:
            CoroHdl hdl;
        public:
            struct promise_type {
                CoroPool* poolPtr = nullptr;   // if not null, lifetime is controlled by pool
                CoroHdl contHdl = nullptr;     // coro that awaits this coro

                CoroPoolTask get_return_object() noexcept {
                    return CoroPoolTask{ CoroHdl::from_promise(*this) };
                }
                auto initial_suspend() const noexcept { return std::suspend_always{}; }
                void unhandled_exception() noexcept { std::exit(1); }
                void return_void() noexcept { }

                auto final_suspend() const noexcept {
                    struct FinalAwaiter {
                        bool await_ready() const noexcept { return false; }
                        std::coroutine_handle<> await_suspend(CoroHdl h) noexcept {
                            if (h.promise().contHdl) {
                                return h.promise().contHdl;    // resume continuation
                            }
                            else {
                                return std::noop_coroutine();  // no continuation
                            }
                        }
                        void await_resume() noexcept { }
                    };
                    return FinalAwaiter{};  // AFTER suspended, resume continuation if there is one
                }
            };

            explicit CoroPoolTask(CoroHdl handle)
                : hdl{ handle } {
            }
            ~CoroPoolTask() {
                if (hdl && !hdl.promise().poolPtr) {
                    // task was not passed to pool:
                    hdl.destroy();
                }
            }
            CoroPoolTask(const CoroPoolTask&) = delete;
            CoroPoolTask& operator= (const CoroPoolTask&) = delete;
            CoroPoolTask(CoroPoolTask&& t)
                : hdl{ t.hdl } {
                t.hdl = nullptr;
            }
            CoroPoolTask& operator= (CoroPoolTask&&) = delete;

            // Awaiter for: co_await task()
            // - queues the new coro in the pool
            // - sets the calling coro as continuation
            struct CoAwaitAwaiter {
                CoroHdl newHdl;
                bool await_ready() const noexcept { return false; }
                void await_suspend(CoroHdl awaitingHdl) noexcept;  // see below
                void await_resume() noexcept {}
            };
            auto operator co_await() noexcept {
                return CoAwaitAwaiter{ std::exchange(hdl, nullptr) }; // pool takes ownership of hdl
            }
        };

        export class CoroPool
        {
        private:
            std::list<std::jthread> threads;         // list of threads
            std::list<CoroPoolTask::CoroHdl> coros;  // queue of scheduled coros
            std::mutex corosMx;
            std::condition_variable_any corosCV;
            std::atomic<int> numCoros = 0;           // counter for all coros owned by the pool

        public:
            explicit CoroPool(int num) {
                // start pool with num threads:
                for (int i = 0; i < num; ++i) {
                    std::jthread worker_thread{ [this](std::stop_token st) {
                                                 threadLoop(st);
                                               } };
                    threads.push_back(std::move(worker_thread));
                }
            }

            ~CoroPool() {
                for (auto& t : threads) {  // request stop for all threads
                    t.request_stop();
                }
                for (auto& t : threads) {  // wait for end of all threads
                    t.join();
                }
                for (auto& c : coros) {    // destroy remaining coros
                    c.destroy();
                }
            }
            CoroPool(CoroPool&) = delete;
            CoroPool& operator=(CoroPool&) = delete;

            void runTask(CoroPoolTask&& coroTask) noexcept {
                auto hdl = std::exchange(coroTask.hdl, nullptr); // pool takes ownership of hdl
                if (hdl.done()) {
                    hdl.destroy();  // OOPS, a done() coroutine was passed
                }
                else {
                    runCoro(hdl);   // schedule coroutine in the pool
                }
            }

            // runCoro(): let pool run (and control lifetime of) coroutine
            // called from:
            // - pool.runTask(CoroPoolTask)
            // - co_await task()
            void runCoro(CoroPoolTask::CoroHdl coro) noexcept {
                ++numCoros;
                coro.promise().poolPtr = this;  // disables destroy in CoroPoolTask
                {
                    std::scoped_lock lock(corosMx);
                    coros.push_front(coro);       // queue coro
                    corosCV.notify_one();         // and let one thread resume it
                }
            }

            void threadLoop(std::stop_token st) {
                while (!st.stop_requested()) {
                    // get next coro task from the queue:
                    CoroPoolTask::CoroHdl coro;
                    {
                        std::unique_lock lock(corosMx);
                        if (!corosCV.wait(lock, st, [&] {
                            return !coros.empty();
                            })) {
                            return;  // stop requested
                        }
                        coro = coros.back();
                        coros.pop_back();
                    }

                    // resume it:
                    coro.resume();  // RESUME

                    // NOTE: The coro initially resumed on this thread might NOT be the coro finally called.
                    // If a main coro awaits a sub coro, then the thread that finally resumed the sub coro
                    // resumes the main coro as its continuation.
                    // => After this resumption, this coro and SOME continuations MIGHT be done
                    std::function<void(CoroPoolTask::CoroHdl)> destroyDone;
                    destroyDone = [&destroyDone, this](auto hdl) {
                        if (hdl && hdl.done()) {
                            auto nextHdl = hdl.promise().contHdl;
                            hdl.destroy();         // destroy handle done
                            --numCoros;            // adjust total number of coros
                            destroyDone(nextHdl);  // do it for all continuations done
                        }
                        };
                    destroyDone(coro);      // recursively destroy coroutines done
                    numCoros.notify_all();  // wake up any waiting waitUntilNoCoros()

                    // sleep a little to force another thread to be used next:
                    std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
                }
            }

            void waitUntilNoCoros() {
                int num = numCoros.load();
                while (num > 0) {
                    numCoros.wait(num);  // wait for notification that numCoros changed the value
                    num = numCoros.load();
                }
            }
        };

        // CoroPoolTask awaiter for: co_await task()
        // - queues the new coro in the pool
        // - sets the calling coro as continuation
        void CoroPoolTask::CoAwaitAwaiter::await_suspend(CoroHdl awaitingHdl) noexcept
        {
            newHdl.promise().contHdl = awaitingHdl;
            awaitingHdl.promise().poolPtr->runCoro(newHdl);
        }
    }
}