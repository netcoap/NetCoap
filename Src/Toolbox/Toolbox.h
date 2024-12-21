//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

#pragma once

#include <string>

#include "NetCoap.h"

#define LIB_MSG_ERR(...)                                                                            \
    netcoap::toolbox::libMsgLog().log(netcoap::toolbox::LogMessage::LogLevel::ERR,                  \
            __VA_ARGS__)

#define LIB_MSG_ERR_THROW_EX(...)                                                                   \
    netcoap::toolbox::libMsgLog().logThrowException(netcoap::toolbox::LogMessage::LogLevel::ERR,    \
            __VA_ARGS__)

#define LIB_MSG_WARN(...)                                                                           \
    netcoap::toolbox::libMsgLog().log(netcoap::toolbox::LogMessage::LogLevel::WARN,                 \
            __VA_ARGS__)

#define LIB_MSG_INFO(...)                                                                           \
    netcoap::toolbox::libMsgLog().log(netcoap::toolbox::LogMessage::LogLevel::INFO,                 \
            __VA_ARGS__)

#define LIB_MSG_DEBUG(...)                                                                          \
    netcoap::toolbox::libMsgLog().log(netcoap::toolbox::LogMessage::LogLevel::DEBUG,                \
            __VA_ARGS__)

#define LIB_MSG_DEBUG_HEX_DUMP(...)                                                                 \
    netcoap::toolbox::libMsgLog().hexDump(netcoap::toolbox::LogMessage::LogLevel::DEBUG,            \
            __VA_ARGS__)

using namespace std;

namespace netcoap {
    namespace toolbox {

        class Serialize {

        public:

            Serialize() = default;
            virtual ~Serialize() = default;

            virtual bool serialize(string &str) = 0;

            virtual bool deserialize(const string &str, size_t &strIndex) = 0;
        };

    }
}
