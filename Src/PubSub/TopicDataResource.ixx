//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <iostream>
#include <ranges>

export module PubSub:TopicDataResource;

import Coap;
import Toolbox;
import :Session;
import :TopicCfgResource;

using namespace std;
using namespace netcoap::coap;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace pubsub {

		export class TopicDataResource : public TopicCfgDataResource {
		public:

			TopicDataResource(string uriTopicCfgPath) {
				m_uriTopicCfgPath = uriTopicCfgPath;
			}

			string getResourceType() const {
				return TopicCfgDataResource::RT_CORE_PS_DATA;
			}

			inline string getUriTopicCfgPath() const {
				return m_uriTopicCfgPath;
			}

			void unsubscribe(IpAddress ipAddr) {
				m_subsMap.erase(ipAddr.toString());
			}

			void handleCb(shared_ptr<Message> req, Session* sess) {
				
				if (req->getCode() == Message::CODE::OP_GET) {
					
					if (req->isOptionNumExist(Option::NUMBER::OBSERVE)) {
				
						size_t obs = req->getOptionNum(Option::NUMBER::OBSERVE);
						if (obs == 0) { // Subscribe
							string query = req->getOptionRepeatStr(Option::NUMBER::URI_QUERY, Option::DELIM_QUERY);

							RespSession respSess;
							respSess.sess = sess;
							respSess.token = req->getToken();
							respSess.subsFilter = query;
							m_subsMap[sess->getClientAddr().toString()] = respSess;
						}
						else { // Unsubscribe
							m_subsMap.erase(sess->getClientAddr().toString());
						}

						shared_ptr<Message> ack = req->buildAckResponse();
						sess->sessionSend(req, ack);
					}
				}
				else if (req->getCode() == Message::CODE::OP_PUT) {

					string query = req->getOptionRepeatStr(Option::NUMBER::URI_QUERY, Option::DELIM_QUERY);

					if (req->isOptionNumExist(Option::NUMBER::BLOCK1)) {
						Block block = req->getOptionBlock(Option::NUMBER::BLOCK1);

						shared_ptr<Message> resp = req->buildAckResponse();
						resp->addOptionBlock(Option::NUMBER::BLOCK1, block);
						if (block.getMore()) {

							resp->setCode(Message::CODE::CONTINUE);
						}
						else {
							resp->setCode(Message::CODE::CHANGED);
						}

						sess->sessionSend(req, resp);

						notifySubscribers(req, query, &block);

						return;
					}

					if (req->getType() == Message::TYPE::CONFIRM) {
						shared_ptr<Message> ack = req->buildAckResponse();
						sess->sessionSend(req, ack);
					}

					notifySubscribers(req, query, nullptr);
				}
				else {
					shared_ptr<Message> errMsg = req->buildErrResponse(Message::CODE::NOT_IMPLEMENTED, "");
					sess->sessionSend(req, errMsg);
				}
			}

			void notifySubscribers(shared_ptr<Message> req,
				string query, Block* blk) {

				unordered_set<string> filterSet;
				for (auto filter : views::split(query, Option::DELIM_QUERY)) {
					string filterExpr(filter.begin(), filter.end());
					filterSet.insert(filterExpr);
				}

				uint16_t msgId = Message::getNxtMsgId();
				uint32_t nxtSeq = getNxtSeq();

				for (auto& [key, sess] : m_subsMap) {

					if (sess.isSubsFilterEmpty() || sess.isSubsFilterInSet(filterSet)) {
						shared_ptr<Message> resp(new Message());

						resp->setMsgId(msgId);
						resp->setToken(sess.token);
						resp->setClientAddr(sess.sess->getClientAddr());
						resp->setType(req->getType());
						resp->setCode(Message::CODE::CONTENT);
						resp->addOptionNum(Option::NUMBER::CONTENT_FORMAT,
							static_cast<size_t>(req->getContentFormat()));
						resp->addOptionNum(Option::NUMBER::OBSERVE, nxtSeq);

						if (blk) {
							resp->addOptionBlock(Option::NUMBER::BLOCK2, *blk);
						}

						resp->setPayload(req->getPayload());

						sess.sess->sessionSend(req, resp);
					}
				}
			}

		private:

			class RespSession {
			public:
				string token;
				Session* sess;
				string subsFilter;

				inline bool isSubsFilterEmpty() {
					return (subsFilter.size() == 0);
				}

				bool isSubsFilterInSet(unordered_set<string> &filterSet) {
					if (filterSet.size() == 0) {
						return false;
					}

					for (auto filter : views::split(subsFilter, Option::DELIM_QUERY)) {
						string filterExpr(filter.begin(), filter.end());
						if (filterSet.find(filterExpr) == filterSet.end()) {
							return false;
						}
					}

					return true;
				}
			};

			uint32_t getNxtSeq() {
				m_nxtSeq++;

				return m_nxtSeq;
			}

			uint32_t m_nxtSeq = 2;
			string m_uriTopicCfgPath = "";
			unordered_map<string, RespSession> m_subsMap;
		};
	}
}
