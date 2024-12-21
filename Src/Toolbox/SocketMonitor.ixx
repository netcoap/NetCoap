//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#ifdef _WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <string>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <unordered_map>

#include "Toolbox/Toolbox.h"

export module Toolbox:SocketMonitor;

import :Helper;
import :LogMessage;
import :Socket;

using namespace std;

namespace netcoap {
	namespace toolbox {

        export class SocketRwState;
        export using MapSocketRwStateType = unordered_map<uint64_t, unique_ptr<SocketRwState>>;

		export class SocketRwState {
		public:

            enum class State { NONE = 0, READ_READY = 1, WRITE_READY = 2, ERR = 8 };

            SocketRwState() {}
            ~SocketRwState() {}

            inline void resetRwState() {
                m_state = static_cast<uint8_t>(State::NONE);
            }

            void setState(State state) {
                m_state |= static_cast<uint8_t>(state);
                if (state == State::ERR) {
                    m_totalErr++;
                }
                else if (state != State::NONE) {
                    m_totalErr = 0;
                }
            }

            inline bool isReadable() const {
                return ((m_state & static_cast<uint8_t>(State::READ_READY)) != 0);
            }

            inline bool isWritable() const {
                return ((m_state & static_cast<uint8_t>(State::WRITE_READY)) != 0);
            }

            inline bool isErr() const {
                return ((m_state & static_cast<uint8_t>(State::ERR)) != 0);
            }

            inline uint32_t getTotalErr() const {
                return m_totalErr;
            }

        private:

            uint8_t m_state = static_cast<uint8_t>(State::NONE);
            uint32_t m_totalErr = 0;
		};

		export class SocketMonitor {
		public:

			SocketMonitor(chrono::microseconds monFor_usec) {
				m_monFor_usec = monFor_usec;
			}

            void addRead(const Socket& sock) {
                unique_ptr<SocketRwState> rState = make_unique<SocketRwState>();                
                m_mapReadState.emplace(sock.getSocket(), std::move(rState));
            }

            void addWrite(const Socket& sock) {
                unique_ptr<SocketRwState> wState = make_unique<SocketRwState>();
                m_mapWriteState.emplace(sock.getSocket(), std::move(wState));
            }

            const SocketRwState* getReadState(const Socket& sock) {
                auto iter = m_mapReadState.find(sock.getSocket());
                if (iter != m_mapReadState.end()) {
                    return iter->second.get();
                }
                else {
                    return nullptr;
                }
            }

            const SocketRwState* getWriteState(const Socket& sock) {
                auto iter = m_mapWriteState.find(sock.getSocket());
                if (iter != m_mapWriteState.end()) {
                    return iter->second.get();
                }
                else {
                    return nullptr;
                }
            }

            int monitorAll() {

                fd_set readFds, writeFds, exceptFds;
                uint64_t maxFd = 0;

                FD_ZERO(&readFds); FD_ZERO(&writeFds); FD_ZERO(&exceptFds);

                for (const auto& [sock, state] : m_mapReadState) {
                    state->resetRwState();
                    FD_SET(sock, &readFds);
                    FD_SET(sock, &exceptFds);
                    if (sock > maxFd) {
                        maxFd = sock;
                    }
                }

                for (const auto& [sock, state] : m_mapWriteState) {
                    state->resetRwState();
                    FD_SET(sock, &writeFds);
                    FD_SET(sock, &exceptFds);
                    if (sock > maxFd) {
                        maxFd = sock;
                    }
                }

                struct timeval selTimeout;
                selTimeout.tv_sec = 0;
                selTimeout.tv_usec = (long) m_monFor_usec.count();
                int total3Fds = select((int) (maxFd + 1), &readFds, &writeFds, &exceptFds, &selTimeout);
                if (total3Fds < 0) {
                    int errNo;
                    #ifdef _WIN32
                    errNo = WSAGetLastError();
                    #else
                    errNo = errno;
                    #endif
                    LIB_MSG_ERR("Select err {}\n", errNo);
                    return -1;
                }
                else if (total3Fds == 0) {
                    return 0;
                }

                for (const auto& [sock, state] : m_mapReadState) {
                    if (FD_ISSET(sock, &exceptFds)) {
                        state->setState(SocketRwState::State::ERR);
                        continue;
                    }
                    if (FD_ISSET(sock, &readFds)) {
                        state->setState(SocketRwState::State::READ_READY);
                    }
                }

                for (const auto& [sock, state] : m_mapWriteState) {
                    if (FD_ISSET(sock, &exceptFds)) {
                        state->setState(SocketRwState::State::ERR);
                        continue;
                    }
                    if (FD_ISSET(sock, &writeFds)) {
                        state->setState(SocketRwState::State::WRITE_READY);
                    }
                }

                return 0;
            }

		private:

            MapSocketRwStateType m_mapReadState;
            MapSocketRwStateType m_mapWriteState;
			chrono::microseconds m_monFor_usec;
		};
	}
}