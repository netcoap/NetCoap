//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <atomic>
#include <iostream>
#include <deque>
#include <mutex>
#include <functional>
#include <atomic>
#include <memory>
#include <optional>
#include <iostream>
#include <thread>

export module Toolbox:SyncQ;

using namespace std;

namespace netcoap {
    namespace toolbox {

        export template<typename T>
        class SyncQ {
        public:

            void push(T& item) {
                lock_guard<mutex> lock(m_mutex);
                m_deque.push_back(item);
            }

            void pushFront(T& item) {
                lock_guard<mutex> lock(m_mutex);
                m_deque.push_front(item);
            }

            bool pop(T& item) {
                lock_guard<mutex> lock(m_mutex);
                if (m_deque.empty()) {
                    return false;
                }

                item = m_deque.front();
                m_deque.pop_front();

                return true;
            }

            bool popBack(T& item) {
                lock_guard<mutex> lock(m_mutex);
                if (m_deque.empty()) {
                    return false;
                }

                item = m_deque.back();
                m_deque.pop_back();

                return true;
            }

            bool empty() const {
                lock_guard<mutex> lock(m_mutex);
                return m_deque.empty();
            }

        private:

            mutable mutex m_mutex;
            deque<T> m_deque;
        };
    }
}