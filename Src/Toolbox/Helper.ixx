//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <algorithm>
#include <iostream>
#include <string>
#include <ranges>
#include <cctype>
#include <syncstream>
#include <ctime>
#include <random>
#include <atomic>

#include "Toolbox/Toolbox.h"

export module Toolbox:Helper;

using namespace std;

namespace netcoap {
    namespace toolbox {

        export class Helper {
        public:

            inline static void removeCh(string &str, unsigned char ch) {
                str.erase(remove(str.begin(), str.end(), ch), str.end());
            }

            inline static string toLower(const string& str) {
                string result = str;
                transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
                    return std::tolower(c);
                    });
                return result;
            }

            inline static void ltrim(string& str) {
                str.erase(str.begin(), ranges::find_if(str, [](char ch) -> bool { return !isspace(ch); }));
            }

            inline static void rtrim(string& str) {
                str.erase(ranges::find_if(str.rbegin(), str.rend(), [](char ch) -> bool { return !isspace(ch); }).base(), str.end());
            }

            inline static void trim(string& str) {
                ltrim(str);
                rtrim(str);
            }

            inline static auto syncOut(ostream& strm = cout) {
                return osyncstream{ strm };
            }

            static uint16_t genRand16() {
                // Create a random device for generating random entropy
                random_device rd;

                // Use a Mersenne Twister generator with the random device as the seed
                mt19937 gen(rd());

                // Define the range for a 16-bit unsigned integer (0 to 65535)
                uniform_int_distribution<uint16_t> dist(0, 65535);

                // Generate a random 16-bit value
                uint16_t randomValue = dist(gen);

                return randomValue;
            }

            static string generateUniqueToken8() {

                static atomic<uint32_t> counter(0);

                time_t currentTime = time(nullptr); // Get current time (seconds since epoch)
                uint32_t count = counter.fetch_add(1); // Increment counter (ensures uniqueness)

                random_device rd;
                mt19937_64 gen(rd());
                uint64_t randomValue = gen(); // Generate a random number

                vector<uint8_t> token;

                // Add 4 bytes of time_t (time component)
                for (int i = 0; i < 4; ++i) {
                    token.push_back((currentTime >> (i * 8)) & 0xFF);
                }

                // Add 2 bytes of counter (ensures uniqueness within a second)
                token.push_back((count >> 8) & 0xFF);
                token.push_back(count & 0xFF);

                // Add 2 bytes of random value (adds unpredictability)
                token.push_back((randomValue >> 8) & 0xFF);
                token.push_back(randomValue & 0xFF);

                return string(token.begin(), token.end()); // 8-byte token string
            }
        };
    }
}
