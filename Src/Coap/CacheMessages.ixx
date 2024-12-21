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

				CacheMessage cacheRespMsg;

				uint8_t code = static_cast<uint8_t>(msg->getCode());
				if (!(code & 0xE0)) { // Msg is a request

					auto iterMsg = m_msgRespTokenMap.find(msg->getToken());
					if (iterMsg != m_msgRespTokenMap.end()) { // Return cache response
						cacheRespMsg = iterMsg->second;
					}
					else {
						cacheRespMsg.respMsg = nullptr;
					}
				}

				return cacheRespMsg.respMsg;
			}

			void updateResponse(shared_ptr<Message> msg) {

				uint8_t code = static_cast<uint8_t>(msg->getCode());
				if ((code & 0xE0) != 0) { // Msg is not a request; cache only responses

					if (msg->getCode() == Message::CODE::EMPTY) {
						return;
					}

					CacheMessage cacheRespMsg;
					time_point tpLastCk = high_resolution_clock::now();
					seconds lastCk_sec = duration_cast<seconds>(tpLastCk.time_since_epoch());
					cacheRespMsg.respMsg = msg;
					m_msgRespTokenMap[msg->getToken()] = cacheRespMsg;
					m_msgTimeMap.insert({ lastCk_sec.count(), msg->getToken() });
				}
			}

			void clean() {
				
				unordered_map<string, time_t> removeMsgIdMap;

				time_point<high_resolution_clock> now = high_resolution_clock::now();
				seconds now_sec = duration_cast<seconds>(now.time_since_epoch());
				for (auto& [lastCk_sec, msgToken] : m_msgTimeMap) {
					time_t durCache_sec = now_sec.count() - lastCk_sec;
					if (durCache_sec >= m_cacheTimeout_sec) {
						removeMsgIdMap[msgToken] = lastCk_sec;
					}
					else {
						break;
					}
				}

				for (auto& [msgToken, lastCk_sec] : removeMsgIdMap) {
					m_msgRespTokenMap.erase(msgToken);
					m_msgTimeMap.erase(lastCk_sec);
				}
			}

		private:

			class CacheMessage {
			public:
				shared_ptr<Message> respMsg = nullptr;
			};

			uint8_t m_cacheTimeout_sec = 0;
			unordered_map<string, CacheMessage> m_msgRespTokenMap;
			multimap<time_t, string> m_msgTimeMap;
		};
	}
}