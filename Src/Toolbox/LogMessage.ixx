//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <chrono>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <format>

#include "Toolbox/Toolbox.h"

#define OUT_STREAM(x)               \
    if (m_outStream.is_open()) {    \
        m_outStream << x;           \
    }                               \
    else {                          \
        cout << x;                  \
    }

export module Toolbox:LogMessage;

import :Helper;

using namespace std;

namespace fs = std::filesystem;
namespace cn = std::chrono;

namespace netcoap {
    namespace toolbox {

        export class LogMessage {
        public:

            enum class LogLevel { ERR, WARN, INFO, DEBUG };

            LogMessage(const string_view logId, const string_view logPathname = "") {

                m_logId = logId;

                if (!logPathname.empty()) {
                    createFile(logPathname);
                }
            }

            ~LogMessage() {
                m_outStream.close();
            }

            void setLogLevel(const LogLevel level) {
                m_logLevel = level;
            }

            template<typename... Args>
            void log(LogLevel level, const string& sformat, Args&&... args) {

                if (level > m_logLevel) {
                    return;
                }

                string outBuf = formatStr(sformat, args...);
                OUT_STREAM(outBuf);
            }

            template<typename... Args>
            void logThrowException(LogLevel level, const string& sformat, Args&&... args) {

                string outBuf = formatStr(sformat, args...);
                if (level <= m_logLevel) {
                    OUT_STREAM(outBuf);
                }

                throw runtime_error(outBuf);
            }

            void hexDump(
                const LogLevel level, const string desc,
                const char* buf, size_t bufSize, const uint8_t perLine = 16) {

                if ((level > m_logLevel) || (bufSize <= 0)) {
                    return;
                }

                if (perLine <= 0) {
                    OUT_STREAM(m_logId + desc + " Empty view of buf caused perLine is not >= 1\n");
                    return;
                }

                string hexBuf = m_logId + " " + desc + "\n" + m_logId + " ";
                for (size_t i = 0; i < bufSize; i++) {
                    
                    hexBuf += std::format("{:02X} ", (unsigned char) buf[i]);

                    if ((i + 1) % perLine == 0) {
                        hexBuf += '\n' + m_logId + " ";
                    }
                }

                if (bufSize % perLine != 0) {
                    hexBuf += '\n';
                }

                OUT_STREAM(hexBuf);
            }

        private:

            template<typename... Args>
            inline string formatStr(const string& sformat, Args&&... args) {

                auto formatArgs = make_format_args(args...);
                string newFormatStr{ m_logId + " " + sformat };

                string outBuf = vformat(newFormatStr, formatArgs) + "\n";

                return outBuf;
            }

            void createFile(string_view pathname) {

                fs::path pathPathname{ pathname };

                // Check for fname is directory or special file such as device file
                if (!pathPathname.has_filename() ||
                    (fs::exists(pathPathname) &&
                        !fs::is_regular_file(pathPathname))) {
                    throw runtime_error("Illegal file name '" + string(pathname) + "' or existed special file");
                }

                // Rename the file if existed
                if (fs::exists(pathPathname)) {
                    cn::high_resolution_clock::time_point now = cn::high_resolution_clock::now();
                    cn::high_resolution_clock::duration dur =
                        cn::duration_cast<cn::microseconds>(now.time_since_epoch());
                    string usecStr{ to_string(dur.count()) };

                    fs::path newPath{ pathPathname.string() + "." + usecStr };

                    error_code ec{};
                    fs::rename(pathPathname, newPath, ec);
                    if (ec) {
                        throw runtime_error("Unable to rename file from '" +
                            pathPathname.string() + "' to '" + newPath.string() + "'");
                    }
                }

                // Open new file to write
                m_outStream.open(pathPathname);
                if (!m_outStream) {
                    throw runtime_error("Unable to create file " + pathPathname.string());
                }
            }

            LogLevel m_logLevel{ LogLevel::DEBUG };
            ofstream m_outStream{};
            string m_logId;
        };

        export LogMessage& libMsgLog() {
            static LogMessage libMsg{ string(NETCOAP) };

            return libMsg;
        }
   }
}