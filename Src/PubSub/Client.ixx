//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <unordered_set>

#include "Toolbox/Toolbox.h"
#include "Coap/Coap.h"

export module PubSub:Client;

import Coap;
import Toolbox;
import :TopicCfgResource;
import :Broker;

using namespace std;
using namespace chrono;
using namespace netcoap::coap;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace pubsub {

		export class Client {
		public:

			enum class STATE { NONE, TRY_CONNECT, ERR_CONNECT, CONNECTED };

			enum class STATUS { SUCCESS, FAILED, MAX_RETRY_EXCEED};

			using ResponseCb = function<void(
				STATUS status, const shared_ptr<Message> respMsg)>;

			Client(JsonPropTree& cfg, IoSession& io) :
				m_io(io), m_cfg(cfg) {

				m_nstart = NSTART;
			}

			STATE getState() {
				return m_state;
			}

			void disconnect() {
				m_stop = true;
			}

			bool connect() {

				m_io.init(m_cfg);

				const uint8_t MAX_TRY_CONNECT = 1;
				m_state = STATE::TRY_CONNECT; uint8_t nConnect = 0;

				while ((nConnect < MAX_TRY_CONNECT) && !m_io.connect(nullptr)) {
					nConnect++;
				}

				if (nConnect == MAX_TRY_CONNECT) {
					m_state = STATE::ERR_CONNECT;
					LIB_MSG_ERR("Error connection to server");
					return false;
				}

				m_state = STATE::CONNECTED;

				m_clientThread = jthread(&Client::run, this);

				return true;
			}

			bool getAllTopicCollection(ResponseCb cb) {

				shared_ptr<Message> req = buildDiscoveryReq(
					Broker::WELL_KNOWN_CORE, TopicCfgDataResource::RT_CORE_PS_COLL);
				
				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool getAllTopicCfgFromCollection(ResponseCb cb) {

				shared_ptr<Message> req = buildDiscoveryReq(
					Broker::WELL_KNOWN_CORE, TopicCfgDataResource::RT_CORE_PS_CONF);
				
				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool getAllTopicData(string uriPath, ResponseCb cb) {

				shared_ptr<Message> req = buildDiscoveryReq(
					uriPath, TopicCfgDataResource::RT_CORE_PS_DATA);
				
				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool getAllTopicCfg(string uriPath, ResponseCb cb) {
				 
				shared_ptr<Message> req(new Message());

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_GET);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriPath, Option::DELIM_PATH);

				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool getAllTopicCfgByProp(string uriPath, JsonPropTree& props, ResponseCb cb) {

				shared_ptr<Message> req(new Message());
				shared_ptr<string> payload(new string());
				props.toCborStr(*payload);

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_FETCH);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriPath, Option::DELIM_PATH);
				req->setPayload(payload);

				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool getTopicCfg(string uriCfgPath, ResponseCb cb) {

				shared_ptr<Message> req(new Message());

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_GET);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriCfgPath, Option::DELIM_PATH);

				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool getTopicCfgByProp(string uriCfgPath, JsonPropTree& props, ResponseCb cb) {

				shared_ptr<Message> req(new Message());
				shared_ptr<string> payload(new string());
				props.toCborStr(*payload);

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_FETCH);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriCfgPath, Option::DELIM_PATH);
				req->setPayload(payload);

				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool setTopicCfgByProp(string uriCfgPath, JsonPropTree& props, ResponseCb cb) {

				shared_ptr<Message> req(new Message());
				shared_ptr<string> payload(new string());
				props.toCborStr(*payload);

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_IPATCH);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriCfgPath, Option::DELIM_PATH);
				req->setPayload(payload);

				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			bool publish(
				string uriDataPath,
				shared_ptr<string> data,
				Message::CONTENT_FORMAT contentFormat,
				bool reliable = false) {

				shared_ptr<Message> req(new Message());

				if (reliable) {
					req->setType(Message::TYPE::CONFIRM);
				} else {
					req->setType(Message::TYPE::NON_CONFIRM);
				}

				req->setCode(Message::CODE::OP_PUT);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriDataPath, Option::DELIM_PATH);
				req->addOptionNum(Option::NUMBER::CONTENT_FORMAT, static_cast<size_t>(contentFormat));
				req->setPayload(data);

				TokenCbDat cbDat(nullptr, TokenCbDat::CALLBACK_TYPE::NONE, req);

				submitReq(cbDat);

				return true;
			}

			bool subscribe(string uriDataPath, ResponseCb cb) {

				if (m_subsTokenMap.find(uriDataPath) != m_subsTokenMap.end()) {
					LIB_MSG_WARN("Already subscribed to {}", uriDataPath);
					return false;
				}

				shared_ptr<Message> req(new Message());

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_GET);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriDataPath, Option::DELIM_PATH);
				req->addOptionNum(Option::NUMBER::OBSERVE, 0);

				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::RECURRENT, req);

				string token = submitReq(cbDat);
				m_subsTokenMap[uriDataPath] = token;

				return true;
			}

			bool unsubscribe(string uriDataPath) {

				shared_ptr<Message> req(new Message());

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_GET);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriDataPath, Option::DELIM_PATH);
				req->addOptionNum(Option::NUMBER::OBSERVE, 1);

				TokenCbDat cbDat(nullptr, TokenCbDat::CALLBACK_TYPE::NONE, req);

				submitReq(cbDat);

				if (m_subsTokenMap.find(uriDataPath) != m_subsTokenMap.end()) {
					lock_guard<mutex> lock(m_mtxToken);
					m_tokenToCbMap.erase(m_subsTokenMap[uriDataPath]);

					m_subsTokenMap.erase(uriDataPath);
				}

				return true;
			}

			//bool getLatestData(string uriDataPath, ResponseCb cb) {

			//	shared_ptr<Message> req(new Message());

			//	req->setType(Message::TYPE::CONFIRM);
			//	req->setCode(Message::CODE::OP_GET);
			//	req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriDataPath, Option::DELIM_PATH);

			//	TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

			//	submitReq(cbDat);

			//	return true;
			//}

			bool removeTopicCfgData(string uriCfgDataPath) {

				shared_ptr<Message> req(new Message());

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_DELETE);
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriCfgDataPath, Option::DELIM_PATH);

				TokenCbDat cbDat(nullptr, TokenCbDat::CALLBACK_TYPE::NONE, req);

				submitReq(cbDat);

				return true;
			}

			bool createTopic(
				string topicName,
				string topicUriPath,
				string dataUriPath,
				string topicType,
				Message::CONTENT_FORMAT mediaType,
				ResponseCb cb) {
				
				size_t pos = dataUriPath.find(topicUriPath);
				if (pos != 0) {
					LIB_MSG_ERR_THROW_EX("Recommend to place dataUriPath (e.g: /ps/data) under topicUriPath (e.g: /ps)");
					return false;
				}

				shared_ptr<Message> req(new Message());

				string json =
					string("{") +
						'"' + TopicCfgResource::TOPIC_NAME + '"' + ':' + '"' + topicName + '"' + ',' +
						'"' + TopicCfgResource::TOPIC_DATA + '"' + ':' + '"' + dataUriPath + '"' + ',' +
						'"' + TopicCfgResource::RESOURCE_TYPE + '"' + ':' + '"' + TopicCfgDataResource::RT_CORE_PS_CONF + '"' + ',' +
						'"' + TopicCfgResource::TOPIC_TYPE + '"' + ':' + '"' + topicType + '"' + ',' +
						'"' + TopicCfgResource::TOPIC_MEDIA_TYPE + '"' + ':' + to_string(static_cast<int>(mediaType)) +
					string("}");

				JsonPropTree jsonTree;
				jsonTree.fromJsonStr(json);
				shared_ptr<string> cbor(new string());
				jsonTree.toCborStr(*cbor);

				req->setType(Message::TYPE::CONFIRM);
				req->setCode(Message::CODE::OP_POST);
				req->addOptionNum(Option::NUMBER::CONTENT_FORMAT, static_cast<size_t>(Message::CONTENT_FORMAT::APP_CBOR));
				req->addOptionRepeatStr(Option::NUMBER::URI_PATH, topicUriPath, Option::DELIM_PATH);
				req->setPayload(cbor);

				TokenCbDat cbDat(cb, TokenCbDat::CALLBACK_TYPE::ONCE, req);

				submitReq(cbDat);

				return true;
			}

			void run() {

				if (m_state != STATE::CONNECTED) {
					LIB_MSG_ERR_THROW_EX("Client not connected to server");
					return;
				}

				while (!m_stop) {
					
					m_io.getData();

					shared_ptr<IoBuf> buf;
					if (m_io.read(buf, nullptr) > 0) {
						procRespMsg(buf);
					}

					if (m_nstart > 0) {
						shared_ptr<Message> msg;
						if (m_reqQ.pop(msg)) {
							procReqMsg(msg);
						}
					}
					else {
						retryXmit();
					}

					m_io.outDataToNet();
				}

				m_state = STATE::NONE;
			}

			void procRespMsg(shared_ptr<IoBuf> buf) {
				
				shared_ptr<Message> resp(new Message());
				size_t idx = 0;
				resp->deserialize(buf->buf, idx);

				if ((resp->getCode() == Message::CODE::EMPTY) &&
					(resp->getType() == Message::TYPE::CONFIRM)) { // Ping message

					shared_ptr<Message> pong = resp->buildResetResponse();
					shared_ptr<IoBuf> buf(new IoBuf());
					pong->serialize(buf->buf);
					m_io.write(buf, nullptr);

					return;
				}

				if (m_respWaitMap.find(resp->getToken()) != m_respWaitMap.end()) {
					m_respWaitMap.erase(resp->getToken());
					m_nstart++;
				}

				bool cont;
				shared_ptr<Message> req = m_block2Xfer.clientRcv(resp, cont);
				if (req) {
					m_reqQ.pushFront(req);
					return;
				}
				else if (resp->getType() == Message::TYPE::CONFIRM) {
					shared_ptr<Message> ack = resp->buildAckResponse();
					shared_ptr<IoBuf> buf(new IoBuf());
					ack->serialize(buf->buf);
					m_io.write(buf, nullptr);
				}

				if (!cont) {
					return;
				}

				if (resp->isOptionNumExist(Option::NUMBER::BLOCK1)) {

					shared_ptr<Message> blockMsg = m_block1Xfer.rcv(resp); // Block 1 more transfer
					if (blockMsg) {
						m_reqQ.pushFront(blockMsg);
						return;
					}
					else {
						blockMsg = resp;
					}
				}

				TokenCbDat cbDat;
				{
					lock_guard<mutex> lock(m_mtxToken);
					cbDat = m_tokenToCbMap[resp->getToken()];
				}

				if ((cbDat.cbType != TokenCbDat::CALLBACK_TYPE::NONE) &&
					(resp->getCode() != Message::CODE::EMPTY)) {

					int code = static_cast<int>(resp->getCode()) & 0xE0;

					if ((code == 0x80) || (code == 0xA0)) { // Class 4 or Class 5 error
						cbDat.cb(STATUS::FAILED, resp);
					}
					else {
						cbDat.cb(STATUS::SUCCESS, resp);
					}

					if (cbDat.cbType != TokenCbDat::CALLBACK_TYPE::RECURRENT) {
						lock_guard<mutex> lock(m_mtxToken);
						m_tokenToCbMap.erase(resp->getToken());
					}
				}
			}

			void procReqMsg(shared_ptr<Message> msg) {

				msg = m_block1Xfer.xfer(msg);

				m_block2Xfer.saveReq(msg);

				shared_ptr<IoBuf> buf;

				buf = make_shared<IoBuf>();
				msg->serialize(buf->buf);
				m_io.write(buf, nullptr);

				ResponseWait respWait;
				respWait.prevTimeWait = high_resolution_clock::now();
				respWait.nxtTimeWaitDur_sec = ACK_TIMEOUT_sec;
				respWait.buf = buf;

				m_respWaitMap[msg->getToken()] = respWait;

				m_nstart--;
			}

		private:

			class TokenCbDat {
			public:
				enum class CALLBACK_TYPE { NONE, ONCE, RECURRENT };

				TokenCbDat() {}

				TokenCbDat(ResponseCb rcp, CALLBACK_TYPE cbt, shared_ptr<Message> m) {
					cb = rcp;
					cbType = cbt;
					msg = m;
				}

				CALLBACK_TYPE cbType = CALLBACK_TYPE::NONE;
				ResponseCb cb;
				shared_ptr<Message> msg;
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

			string submitReq(TokenCbDat cbDat) {

				string token = Message::getNxtToken();
				uint16_t msgId = Message::getNxtMsgId();

				cbDat.msg->setMsgId(msgId);
				cbDat.msg->setToken(token);

				{
					lock_guard<mutex> lock(m_mtxToken);
					m_tokenToCbMap[token] = cbDat;
				}

				m_reqQ.push(cbDat.msg);

				return token;
			}

			shared_ptr<Message> buildDiscoveryReq(string uriPath, string uriQuery) 
			{
				shared_ptr<Message> reqMsg(new Message());

				reqMsg->setType(Message::TYPE::CONFIRM);
				reqMsg->setCode(Message::CODE::OP_GET);
				reqMsg->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriPath, Option::DELIM_PATH);
				reqMsg->addOptionRepeatStr(Option::NUMBER::URI_QUERY, uriQuery, Option::DELIM_QUERY);

				return reqMsg;
			}

			IoSession& m_io;
			JsonPropTree& m_cfg;
			STATE m_state = STATE::NONE;
			bool m_stop = false;
			uint8_t m_nstart;
			SyncQ<shared_ptr<Message>> m_reqQ;
			unordered_map<string, ResponseWait> m_respWaitMap;
			mutex m_mtxToken;
			unordered_map<string, TokenCbDat> m_tokenToCbMap;
			unordered_map<string, string> m_subsTokenMap;
			jthread m_clientThread;
			Block1Xfer m_block1Xfer;
			Block2Xfer m_block2Xfer;
		};
	}
}