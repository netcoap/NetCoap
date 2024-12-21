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
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include <format>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cstring>

#include "Toolbox/Toolbox.h"

export module Toolbox:Socket;

import :Helper;
import :LogMessage;

using namespace std;

namespace netcoap {
	namespace toolbox {

        export typedef union IpAddress {

            enum class IP_FAMILY { UNSPEC = AF_UNSPEC, IP4 = AF_INET, IP6 = AF_INET6 };

            struct sockaddr_in ipAddr4;
            struct sockaddr_in6 ipAddr6;

            IpAddress() {
                memset(this, 0, sizeof(*this));
            }

            IpAddress(const string shost, uint16_t port) {

                memset(this, 0, sizeof(*this));

                struct addrinfo hints, *infoPtr;
                std::memset(&hints, 0, sizeof(hints));

                hints.ai_family = static_cast<uint16_t>(IP_FAMILY::UNSPEC);

                int status = getaddrinfo(shost.c_str(), nullptr, &hints, &infoPtr);
                if (status != 0) {
                    string err = gai_strerror(status);
                    LIB_MSG_ERR_THROW_EX("Invalid IP host {} with err {}", shost, err);
                }

                memcpy(this, infoPtr->ai_addr, infoPtr->ai_addrlen);

                ipAddr6.sin6_port = htons(port);

                freeaddrinfo(infoPtr);
            }

            inline uint16_t getPort() const {
                return ntohs(ipAddr6.sin6_port);
            }

            inline IP_FAMILY getFamily() const {
                return static_cast<IP_FAMILY>(ipAddr6.sin6_family);
            }
            
            inline uint8_t getAddressLen() const {

                switch (getFamily()) {
                    case IP_FAMILY::IP4:
                        return sizeof(ipAddr4);
                    break;

                    case IP_FAMILY::IP6:
                        return sizeof(ipAddr6);
                    break;

                    default:
                    break;
                }

                return 0;
            }

            string getAddress() const {

                void* numericAddr;

                switch (getFamily()) {
                case IP_FAMILY::IP4:
                    numericAddr = (void*) &ipAddr4.sin_addr;
                    break;

                case IP_FAMILY::IP6:
                    numericAddr = (void*)  &ipAddr6.sin6_addr;
                    break;

                default:
                    return "Unknown family: " + to_string(ipAddr6.sin6_family);
                }

                char addrBuf[INET6_ADDRSTRLEN];
                if (!inet_ntop(ipAddr6.sin6_family, numericAddr, addrBuf, sizeof(addrBuf))) {
                    return "Unable to convert invalid address\n";
                }

                return addrBuf;
            }

            string toString() const {

                string sfamily;

                switch (getFamily()) {
                case IP_FAMILY::IP4:
                    sfamily = "IP4";
                    break;

                case IP_FAMILY::IP6:
                    sfamily = "IP6";
                    break;

                default:
                    sfamily = "Unknown family";
                }

                return "family: " + sfamily + "; address: " + getAddress() + "; port: " + to_string(getPort());
            }
        } IpAddress;

		export class Socket {
		public:

            enum class SocketType { UDP = SOCK_DGRAM, TCP = SOCK_STREAM };
            enum class SocketOptLevel { SOCKET = SOL_SOCKET, IPV6 = IPPROTO_IPV6 };
            enum class SocketOptName { REUSEADDR = SO_REUSEADDR, V6ONLY = IPV6_V6ONLY };

			Socket(IpAddress::IP_FAMILY family, SocketType type) {

                m_type = type;
                m_family = family;

                m_sock = socket(static_cast<uint16_t>(m_family), static_cast<int>(m_type), 0);
                if (m_sock == -1) {
                    int errNo;
                    #ifdef _WIN32
                    errNo = WSAGetLastError();
                    #else
                    errNo = errno;
                    #endif

                    LIB_MSG_ERR_THROW_EX("Unable to create socket cause err {}", errNo);
                }
            }

            ~Socket() {
                if (m_sock != -1) {
                    #ifdef _WIN32
                    closesocket(m_sock);
                    #else
                    close(m_sock);
                    #endif
                }
            }

            inline int getSocket() const {
                return m_sock;
            }

            inline SocketType getType() const {
                return m_type;
            }

            inline IpAddress::IP_FAMILY getFamily() const {
                return m_family;
            }

            int connect(const IpAddress& remoteAddr) {
                if (::connect(getSocket(), (struct sockaddr*) &remoteAddr, remoteAddr.getAddressLen())) {
                    int errNo;
                    #ifdef _WIN32
                    errNo = WSAGetLastError();
                    #else
                    errNo = errno;
                    #endif

                    #ifdef _WIN32
                    if ((errNo == WSAEINPROGRESS) || (errNo == WSAEWOULDBLOCK)) {
                    #else
                    if (errNo == EINPROGRESS) {
                    #endif
                        return 0;
                    }

                    LIB_MSG_ERR("Unable to make connection to {} cause err {}\n", remoteAddr.toString(), errNo);
                    return -1;
                }

                return 0;
            }

            int bind(const IpAddress& addr) {

                if (getFamily() == IpAddress::IP_FAMILY::IP6) {
                    int off = 0;
                    if (setOption(SocketOptLevel::IPV6, SocketOptName::V6ONLY, &off, sizeof(off)) != 0) {
                        return -1;
                    }
                }

                if (::bind(getSocket(), (struct sockaddr*) &addr, addr.getAddressLen()) < 0) {
                    int errNo;
                    #ifdef _WIN32
                    errNo = WSAGetLastError();
                    #else
                    errNo = errno;
                    #endif
                    LIB_MSG_ERR("Unable to bind socket to {} cause err {}\n", addr.toString(), errNo);
                    return -1;
                }

                return 0;
            }

            Socket accept() {

                IpAddress clientAddr;
                socklen_t addrSiz = sizeof(IpAddress);

                int clientSock = ::accept(getSocket(), (struct sockaddr*) &clientAddr, &addrSiz);
                if (clientSock < 0) {
                    int errNo;
                    #ifdef _WIN32
                    errNo = WSAGetLastError();
                    #else
                    errNo = errno;
                    #endif
                    LIB_MSG_ERR_THROW_EX("Unable to accept client connection cause err {}\n", errNo);
                }

                Socket socket{ clientSock, clientAddr.getFamily(), getType() };

                return socket;
            }

            int recv(char* buf, size_t len) {

                int bytesRcv;
                while (true) {
                    if ((bytesRcv = ::recv(getSocket(), buf, (int) len, 0)) < 0) {
                        int errNo;
                        #ifdef _WIN32
                        errNo = WSAGetLastError();
                        #else
                        errNo = errno;
                        #endif

                        #ifdef _WIN32
                        if (errNo == WSAEWOULDBLOCK) {
                        #else
                        if ((errNo == EAGAIN) || (errNo == EWOULDBLOCK)) {
                        #endif
                            continue;
                        }

                        LIB_MSG_ERR("Unable to receive message cause err {}\n", errNo);
                        return -1;
                    }
                }

                return bytesRcv;
            }

            int send(const char* buf, size_t len) {
                static const int MAX_RETRIES = 3;
                int retryCount = 0;
                int totalSent = 0;
                int bytesSent;
                int errNo = 0;
                while ((totalSent < len) && (retryCount < MAX_RETRIES)) {
                    if ((bytesSent = ::send(getSocket(), (char*)(buf + totalSent), (int) (len - totalSent), 0)) < 0) {
                        #ifdef _WIN32
                        errNo = WSAGetLastError();
                        #else
                        errNo = errno;
                        #endif

                        #ifdef _WIN32
                        if (errNo == WSAEWOULDBLOCK) {
                        #else
                        if ((errNo == EAGAIN) || (errNo == EWOULDBLOCK)) {
                        #endif
                                retryCount++;
                                continue;
                        }

                        break;
                    }
                    else if (bytesSent == 0) {
                        retryCount++;
                        continue;
                    }

                    totalSent += bytesSent;
                    retryCount = 0;
                }

                if (totalSent != len) {
                    LIB_MSG_ERR("Unable to send message cause err {}\n", errNo);
                    return -1;
                }

                return totalSent;
            }

            int setNonBlocking() {

                #ifdef _WIN32
                unsigned long nonBlocking = 1;
                int errNo;
                if (ioctlsocket(getSocket(), FIONBIO, &nonBlocking) != NO_ERROR) {
                    errNo = WSAGetLastError();
                    LIB_MSG_ERR("Unable to set blocking cause err {}\n", errNo);
                    return -1;
                }
                #else
                int flags = fcntl(getSocket(), F_GETFL, 0);
                if (flags == -1) {
                    LIB_MSG_ERR("Unable to set blocking cause err {}\n", errno);
                    return -1;
                }

                if (fcntl(getSocket(), F_SETFL, flags | O_NONBLOCK) == -1) {
                    LIB_MSG_ERR("Unable to set blocking cause err {}\n", errno);
                    return -1;
                }
                #endif

                return 0;
            }

            int setOption(
                SocketOptLevel level, SocketOptName optName, const void* optval, int optlen) {
                if (setsockopt(getSocket(),
                        static_cast<int>(level), static_cast<int>(optName), (char*) optval, optlen) < 0) {
                    int errNo;
                    #ifdef _WIN32
                    errNo = WSAGetLastError();
                    #else
                    errNo = errno;
                    #endif
                    LIB_MSG_ERR("Unable to set socket option level {} optName {} cause err {}\n",
                        static_cast<int>(level), static_cast<int>(optName), errNo);
                    return -1;
                }

                return 0;
            }

            int getOption(
                SocketOptLevel level, SocketOptName optName, void* optval, socklen_t* optlen) {
                if (getsockopt(getSocket(),
                        static_cast<int>(level), static_cast<int>(optName), (char*) optval, optlen) < 0) {
                    int errNo;
                    #ifdef _WIN32
                    errNo = WSAGetLastError();
                    #else
                    errNo = errno;
                    #endif
                    LIB_MSG_ERR("Unable to get socket option level: {}, optName: {} cause err {}\n",
                        static_cast<int>(level), static_cast<int>(optName), errNo);
                    return -1;
                }

                return 0;
            }

        private:

            int m_sock = 0;
            SocketType m_type;
            IpAddress::IP_FAMILY m_family;

            Socket(int sock, IpAddress::IP_FAMILY family, SocketType type) {
                m_sock = sock;
                m_family = family;
                m_type = type;
            }
		};
	}
}