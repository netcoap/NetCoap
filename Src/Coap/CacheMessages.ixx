//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <iostream>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <map>

export module Coap:CacheMessages;

import :Message;

using namespace std;
using namespace chrono;

namespace netcoap {
	namespace coap {

		export class CacheMessages {
		public:

			inline void setTimeout(uint8_t cacheTimeout_sec) {
				m_cacheTimeout_sec = cacheTimeout_sec;
			}

			shared_ptr<Message> duplicateMsg(shared_ptr<Message> msg) {

				uint8_t code = static_cast<uint8_t>(msg->getCode());
				if (!(code & 0xE0)) { // Msg is a request

					string header;
					msg->serializeHeader(header);

					auto iterMsg = m_msgRespMsgMap.find(header);
					if (iterMsg != m_msgRespMsgMap.end()) { // Return cache response
						return iterMsg->second;
					}
					else {
						m_msgRespMsgMap.emplace(header, nullptr);
					}
				}

				return nullptr;
			}

			void updateResponse(shared_ptr<Message> req, shared_ptr<Message> resp) {

				uint8_t code = static_cast<uint8_t>(resp->getCode());
				if ((code & 0xE0) != 0) { // Msg is not a request; cache only responses

					if (resp->getCode() == Message::CODE::EMPTY) {
						return;
					}

					time_point tpLastCk = high_resolution_clock::now();
					seconds lastCk_sec = duration_cast<seconds>(tpLastCk.time_since_epoch());
					string reqHeader;
					req->serializeHeader(reqHeader);
					m_msgRespMsgMap[reqHeader] = resp;
					m_msgTimeMap.insert({ lastCk_sec.count(), reqHeader });
				}
			}

			void clean() {
				
				unordered_map<string, time_t> removeMsgMap;

				time_point<high_resolution_clock> now = high_resolution_clock::now();
				seconds now_sec = duration_cast<seconds>(now.time_since_epoch());
				for (auto& [lastCk_sec, reqHeader] : m_msgTimeMap) {
					time_t durCache_sec = now_sec.count() - lastCk_sec;
					if (durCache_sec >= m_cacheTimeout_sec) {
						removeMsgMap[reqHeader] = lastCk_sec;
					}
					else {
						break;
					}
				}

				for (auto& [reqHeader, lastCk_sec] : removeMsgMap) {
					m_msgRespMsgMap.erase(reqHeader);
					m_msgTimeMap.erase(lastCk_sec);
				}
			}

		private:

			uint8_t m_cacheTimeout_sec = 0;
			unordered_map<string, shared_ptr<Message>> m_msgRespMsgMap;
			multimap<time_t, string> m_msgTimeMap;
		};
	}
}