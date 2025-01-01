//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <memory>
#include <coroutine>
#include <iostream>
#include <chrono>
#include <thread>
#include <unordered_map>

#include "Toolbox/Toolbox.h"
#include "Coap/Coap.h"

export module PubSub:Session;

import Toolbox;
import Coap;
import :BrokerIf;
import :SessionMessage;

using namespace std;
using namespace chrono;

using namespace netcoap::toolbox;
using namespace netcoap::coap;

namespace netcoap {
	namespace pubsub {

		export class Session {
		public:

			Session(
				IpAddress clientAddr,
				JsonPropTree& cfg, IoSession& io,
				BrokerIf* broker,
				SyncQ<shared_ptr<IoBuf>>* inpQ) :
				m_cfg(cfg), m_io(io) {

				m_io = io;
				m_broker = broker;
				m_inpQ = inpQ;
				m_clientAddr = clientAddr;
				
				m_totalProcTime_us = m_cfg.get<int>("coapPs.serverSessionTotalProcTime_us");
				m_ping.setKeepAlive(
					m_cfg.get<int>("coap.keepAlive_sec")
				);
				m_nstart = NSTART;
				//m_cacheMsgs.setTimeout(CACHE_TIMEOUT_sec);
			}

			~Session() {}

			Session(const Session&) = delete;
			Session& operator=(const Session&) = delete;

			inline bool isStop() {
				return m_stop;
			}

			IpAddress getClientAddr() {
				return m_clientAddr;
			}

			void sessionSend(shared_ptr<Message> req, shared_ptr<Message> resp) {

				ReqRespMsg reqRespMsg;
				reqRespMsg.req = req;
				reqRespMsg.resp = resp;

				m_outpQ.push(reqRespMsg);
			}

			CoroPoolTask run() {

				time_point startProcTime = high_resolution_clock::now();;

				while (!m_stop) {
				
					time_point now = high_resolution_clock::now();
					duration durProcTime_us = duration_cast<microseconds>(now - startProcTime);
					if (durProcTime_us.count() >= m_totalProcTime_us) {
						m_broker->runSession(this);

						break; // Resume at later time
					}

					shared_ptr<IoBuf> buf;
					if (m_inpQ->pop(buf)) {

						m_ping.stampRcvMsg();

						shared_ptr<Message> msg(new Message());
						size_t idx = 0;
						msg->deserialize(buf->buf, idx);
						msg->setClientAddr(buf->clientAddr);

						if ((msg->getType() == Message::TYPE::RESET) ||
							(msg->getType() == Message::TYPE::ACK)) {
							if (m_respWaitMap.find(msg->getToken()) == m_respWaitMap.end()) {
								continue; // Response is already processed and thus ignore duplicate response
							}

							m_nstart++;
							m_respWaitMap.erase(msg->getToken());

							continue;
						}

						//shared_ptr<Message> cacheMsg = m_cacheMsgs.duplicateMsg(msg);
						//if (cacheMsg != nullptr) {
						//	buf = make_shared<IoBuf>();
						//	cacheMsg->serialize(buf->buf);
						//	buf->clientAddr = cacheMsg->getClientAddr();
						//	m_io.write(buf, nullptr);

						//	continue;
						//}

						shared_ptr<Message> ack = m_block2Xfer.serverRcv(msg);
						if (ack) {
							buf = make_shared<IoBuf>();
							ack->serialize(buf->buf);
							buf->clientAddr = ack->getClientAddr();
							m_io.write(buf, nullptr);
						}
						else {
							m_broker->msgRcv(msg);
						}
					}

					if (m_ping.isTime4Ping()) {
						shared_ptr<Message> ping = Message::buildPing();
						ping->setClientAddr(getClientAddr());
						ReqRespMsg reqRespMsg;
						reqRespMsg.resp = ping;
						m_outpQ.pushFront(reqRespMsg);
					}

					if (!m_ping.isClientAlive()) {
						// NO ACK from CONFIRM or PING timeout

						shared_ptr<SessionMessage> sessionMsg(new SessionMessage());
						sessionMsg->setClientAddr(getClientAddr());
						sessionMsg->setRequest(SessionMessage::REQUEST::SESSION_EXIT);
						m_broker->msgRcv(sessionMsg);

						m_stop = true;
						continue;
					}

					if (m_nstart > 0) {
						ReqRespMsg reqRespMsg;
						if (m_outpQ.pop(reqRespMsg)) {
							procRespMsg(reqRespMsg);
						}
					}
					else {
						retryXmit();
					}

					//m_cacheMsgs.clean();
				}

				co_return;
			}

		private:

			class ReqRespMsg {
			public:
				shared_ptr<Message> req = nullptr;
				shared_ptr<Message> resp = nullptr;
			};

			class ResponseWait {
			public:
				uint16_t msgId = 0;
				time_point<high_resolution_clock> prevTimeWait;
				time_t nxtTimeWaitDur_sec = 0;
				uint8_t numRetry = 0;
				shared_ptr<IoBuf> buf;
			};

			void retryXmit() {

				vector<string> overMaxRetransmit;
				for (auto& [token, respWait] : m_respWaitMap) {

					if (respWait.numRetry < MAX_RETRANSMIT) {
						time_point now = high_resolution_clock::now();
						duration durTimeSpentWait_sec = duration_cast<seconds>(now - respWait.prevTimeWait);
						if (durTimeSpentWait_sec.count() > respWait.nxtTimeWaitDur_sec) {

							respWait.numRetry++;
							respWait.nxtTimeWaitDur_sec = (time_t)((2 * respWait.numRetry) * ACK_RANDOM_FACTOR);
							respWait.prevTimeWait = high_resolution_clock::now();

							m_io.write(respWait.buf, nullptr);
						}
					}
					else {
						overMaxRetransmit.push_back(token);
					}
				}

				for (string token : overMaxRetransmit) {
					m_respWaitMap.erase(token);
				}
			}

			void procRespMsg(ReqRespMsg reqRespMsg) {

				shared_ptr<IoBuf> buf;

				//m_cacheMsgs.updateResponse(reqRespMsg.req, reqRespMsg.resp);

				reqRespMsg.resp = m_block2Xfer.serverXfer(reqRespMsg.resp);

				if (reqRespMsg.resp->getType() != Message::TYPE::CONFIRM) {
					buf = make_shared<IoBuf>();
					reqRespMsg.resp->serialize(buf->buf);
					buf->clientAddr = reqRespMsg.resp->getClientAddr();
					m_io.write(buf, nullptr);

					return;
				}

				buf = make_shared<IoBuf>();
				reqRespMsg.resp->serialize(buf->buf);
				buf->clientAddr = reqRespMsg.resp->getClientAddr();
				m_io.write(buf, nullptr);

				ResponseWait respWait;
				respWait.prevTimeWait = high_resolution_clock::now();
				respWait.nxtTimeWaitDur_sec = ACK_TIMEOUT_sec;
				respWait.buf = buf;

				m_respWaitMap[reqRespMsg.resp->getToken()] = respWait;

				m_nstart--;
			}

			JsonPropTree& m_cfg;
			IoSession& m_io;
			IpAddress m_clientAddr;
			SyncQ<ReqRespMsg> m_outpQ;
			SyncQ<shared_ptr<IoBuf>>* m_inpQ;
			uint32_t m_totalProcTime_us = 0;
			BrokerIf* m_broker;
			unordered_map<string, ResponseWait> m_respWaitMap;
			uint8_t m_nstart;
			//CacheMessages m_cacheMsgs;
			Block2Xfer m_block2Xfer;
			Ping m_ping;
			bool m_stop = false;
		};
	}
}