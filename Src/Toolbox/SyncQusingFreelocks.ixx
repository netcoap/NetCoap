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
        private:
            struct Node {
                std::shared_ptr<T> data;
                std::atomic<Node*> next;
                std::atomic<Node*> prev;

                explicit Node(T value)
                    : data(std::make_shared<T>(std::move(value))), next(nullptr), prev(nullptr) {
                }

                Node() : data(nullptr), next(nullptr), prev(nullptr) {}
            };

            std::atomic<Node*> head;
            std::atomic<Node*> tail;

        public:

            SyncQ() {
                Node* dummy = new Node(); // Dummy node for simplicity
                head.store(dummy);
                tail.store(dummy);
            }

            ~SyncQ() {
                while (Node* node = head.load()) {
                    Node* next = node->next.load();
                    delete node;
                    node = next;
                }
            }
            void push(T value) {
                Node* new_node = new Node(std::move(value));

                while (true) {
                    Node* old_tail = tail.load(std::memory_order_acquire);
                    Node* next = old_tail->next.load(std::memory_order_acquire);

                    if (old_tail == tail.load(std::memory_order_acquire)) {
                        if (next == nullptr) {
                            if (old_tail->next.compare_exchange_weak(next, new_node, std::memory_order_release)) {
                                new_node->prev.store(old_tail, std::memory_order_release);
                                tail.compare_exchange_weak(old_tail, new_node, std::memory_order_release);
                                break;
                            }
                        }
                        else {
                            tail.compare_exchange_weak(old_tail, next, std::memory_order_release);
                        }
                    }
                }
            }

            void pushFront(T value) {
                Node* new_node = new Node(std::move(value));

                while (true) {
                    Node* old_head = head.load(std::memory_order_acquire);
                    new_node->next.store(old_head, std::memory_order_release);

                    if (head.compare_exchange_weak(old_head, new_node, std::memory_order_release)) {
                        old_head->prev.store(new_node, std::memory_order_release);
                        break;
                    }
                }
            }

            bool pop(T& value) {
                while (true) {
                    Node* old_head = head.load(std::memory_order_acquire);
                    Node* next = old_head->next.load(std::memory_order_acquire);

                    if (old_head == head.load(std::memory_order_acquire)) {
                        if (next == nullptr) {
                            return false; // Queue is empty
                        }

                        if (head.compare_exchange_weak(old_head, next, std::memory_order_release)) {
                            value = *next->data;
                            delete old_head;

                            return true;
                        }
                    }
                }
            }

            bool popBack(T& value) {
                while (true) {
                    Node* old_tail = tail.load(std::memory_order_acquire);
                    Node* prev = old_tail->prev.load(std::memory_order_acquire);

                    if (old_tail == tail.load(std::memory_order_acquire)) {
                        if (prev == nullptr || prev == head.load()) {
                            return false; // Queue is empty
                        }

                        if (tail.compare_exchange_weak(old_tail, prev, std::memory_order_release)) {
                            value = *old_tail->data;
                            delete old_tail;

                            return true;
                        }
                    }
                }
            }
        };
    }
}