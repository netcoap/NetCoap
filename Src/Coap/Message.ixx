//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <string>
#include <map>
#include <memory>
#include <sstream>
#include <iostream>

#include "Coap/Coap.h"
#include "Toolbox/Toolbox.h"

export module Coap:Message;

import Toolbox;
import :Option;

using namespace std;

using namespace netcoap::toolbox;

namespace netcoap {
	namespace coap {

        export class NetCoapMessage {
		public:

			NetCoapMessage() {}
			virtual ~NetCoapMessage() {}

			IpAddress getClientAddr() {
				return m_clientAddr;
			}

			void setClientAddr(IpAddress ipAddr) {
				m_clientAddr = ipAddr;
			}

		private:

			IpAddress m_clientAddr;
        };

		export class Message : public NetCoapMessage, public Serialize {
		public:

			enum class TYPE { CONFIRM = 0, NON_CONFIRM = 1, ACK = 2, RESET = 3 };

			enum class CODE {
				// Empty message 0.0
				EMPTY = 0,

				// Requests with 0.x such as PUT 0.3
				OP_GET = 1, OP_POST = 2, OP_PUT = 3, OP_DELETE = 4, OP_FETCH = 5, OP_IPATCH = 7,

				// Success Response 2.x from server to indicate that the clients request was
				// successfully received, understood, and accepted
				CREATED = MSG_CODE(201), DELETED = MSG_CODE(202),
				VALID = MSG_CODE(203), CHANGED = MSG_CODE(204), CONTENT = MSG_CODE(205),
				CONTINUE = MSG_CODE(231),

				// Server response to Client errors 4.x
				BAD_REQUEST = MSG_CODE(400), UNAUTHORIZED = MSG_CODE(401), BAD_OPTION = MSG_CODE(402),
				FORBIDDEN = MSG_CODE(403), NOT_FOUND = MSG_CODE(404), METHOD_NOT_ALLOWED = MSG_CODE(405),
				NOT_ACCEPTABLE = MSG_CODE(406), PRECONDITION_FAILED = MSG_CODE(412),
				REQUEST_ENTITY_TOO_LARGE = MSG_CODE(413), UNSUPPORTED_CONTENT_FORMAT = MSG_CODE(415),

				// Server response to client with Server errors 5.x
				INTERNAL_SERVER_ERROR = MSG_CODE(500), NOT_IMPLEMENTED = MSG_CODE(501), BAD_GATEWAY = MSG_CODE(502),
				SERVICE_UNAVAILABLE = MSG_CODE(503), GATEWAY_TIMEOUT = MSG_CODE(504),
				PROXY_NOT_SUPPORTED = MSG_CODE(505)
			};

			enum class CONTENT_FORMAT {
				TEXT_PLAIN = 0, APP_LINK_FORMAT = 40, APP_XML = 41,
				APP_OCTET_STREAM = 42, APP_EXI = 47, APP_JSON = 50,
				APP_CBOR = 256
			};

			const static uint8_t PAYLOAD_MARKER = 0xFF;

			Message() {}
			~Message() {}

			Message(TYPE type) {
				m_type = type;
			}

			inline Message::CONTENT_FORMAT getContentFormat() const {
				Option* optUri = getOption(Option::NUMBER::CONTENT_FORMAT);
				if (!optUri) {
					return Message::CONTENT_FORMAT::TEXT_PLAIN;
				}

				return static_cast<Message::CONTENT_FORMAT>(optUri->getNumVal());
			}

			inline string getOptionRepeatStr(Option::NUMBER optNum, char delim) {

				string s;
				auto range = m_optList.equal_range(optNum);
				for (auto it = range.first; it != range.second; ++it) {
					Option* opt = it->second.get();
					
					s.append(opt->getStrVal() + delim);
				}
				if (!s.empty()) {
					s.pop_back(); // Remove last delim
				}

				return s;
			}

			inline string getOptionStr(Option::NUMBER optNum) {
				Option* opt = getOption(optNum);
				if (!opt) {
					return "";
				}

				return opt->getStrVal();
			}

			inline size_t getOptionNum(Option::NUMBER optNum) {
				Option* opt = getOption(optNum);
				if (!opt) {
					return 0;
				}

				return opt->getNumVal();
			}

			inline Block getOptionBlock(Option::NUMBER optBlock) {
				Block block;
				Option* opt = getOption(optBlock);
				if (!opt) {
					return block;
				}

				return opt->getBlockVal();
			}

			inline bool isOptionNumExist(Option::NUMBER optNum) {
				Option* opt = getOption(optNum);
				if (!opt) {
					return false;
				}

				return true;
			}

			inline TYPE getType() const {
				return m_type;
			}

			inline void setType(TYPE type) {
				m_type = type;
			}

			inline void setToken(string token) {
				m_token = token;
			}

			inline const string& getToken() const {
				return m_token;
			}

			inline CODE getCode() const {
				return m_code;
			}

			inline void setCode(CODE code) {
				m_code = code;
			}

			inline uint16_t getMsgId() const {
				return m_msgId;
			}

			inline void setMsgId(uint16_t msgId) {
				m_msgId = msgId;
			}

			inline shared_ptr<string> getPayload() const {
				return m_payload;
			}

			inline void setPayload(shared_ptr<string> payload) {
				m_payload = payload;
			}

			inline unique_ptr<Option> removeOption(Option::NUMBER numb) {

				auto iter = m_optList.find(numb);
				if (iter == m_optList.end()) {
					return nullptr;
				}

				unique_ptr<Option> opt = std::move(iter->second);
				m_optList.erase(iter);

				return opt;
			}

			inline Option* getOption(Option::NUMBER numb) const {

				auto iter = m_optList.find(numb);
				if (iter == m_optList.end()) {
					return nullptr;
				}

				return iter->second.get();
			}

			inline void addOption(unique_ptr<Option> opt) {
				m_optList.emplace(opt->getNumber(), std::move(opt));
			}

			void addOptionNum(Option::NUMBER optNum, size_t v) {
				unique_ptr<Option> opt(new Option());

				unique_ptr<OptVal<size_t>> optVal(
					dynamic_cast<OptVal<size_t>*>(Option::newVal(optNum).release()));
				optVal->set(v);

				opt->setNumber(optNum);
				opt->setVal(std::move(optVal));

				addOption(std::move(opt));
			}

			void addOptionStr(Option::NUMBER optNum, string s) {
				unique_ptr<Option> opt(new Option());

				unique_ptr<OptVal<string>> optVal(
					dynamic_cast<OptVal<string>*>(Option::newVal(optNum).release()));
				optVal->set(s);

				opt->setNumber(optNum);
				opt->setVal(std::move(optVal));

				addOption(std::move(opt));
			}

			void addOptionRepeatStr(Option::NUMBER optNum, string s, char delim) {

				stringstream ss(s);

				string name;
				while (getline(ss, name, delim)) {
					addOptionStr(optNum, name);
				}
			}

			void addOptionBlock(Option::NUMBER optBlock, Block block) {
				unique_ptr<Option> opt(new Option());

				unique_ptr<OptVal<Block>> optVal(
					dynamic_cast<OptVal<Block>*>(Option::newVal(optBlock).release()));
				optVal->set(block);

				opt->setNumber(optBlock);
				opt->setVal(std::move(optVal));

				addOption(std::move(opt));
			}

			static shared_ptr<Message> buildPing() {
				shared_ptr<Message> ping(new Message);

				ping->setMsgId(getNxtMsgId());
				ping->setToken("");
				ping->setType(Message::TYPE::CONFIRM);
				ping->setCode(Message::CODE::EMPTY);

				return ping;
			}

			shared_ptr<Message> buildResetResponse() {
				shared_ptr<Message> respMsg(new Message);

				respMsg->setMsgId(getMsgId());
				respMsg->setToken("");
				respMsg->setClientAddr(getClientAddr());
				respMsg->setType(Message::TYPE::RESET);
				respMsg->setCode(Message::CODE::EMPTY);

				return respMsg;
			}

			shared_ptr<Message> buildAckResponse() {
				shared_ptr<Message> respMsg(new Message);

				respMsg->setMsgId(getMsgId());
				respMsg->setToken(getToken());
				respMsg->setClientAddr(getClientAddr());
				respMsg->setType(Message::TYPE::ACK);
				respMsg->setCode(Message::CODE::EMPTY);

				return respMsg;
			}

			shared_ptr<Message> buildErrResponse(CODE code, string err) {
				shared_ptr<Message> errMsg(new Message);

				errMsg->setMsgId(getMsgId());
				errMsg->setToken(getToken());
				errMsg->setClientAddr(getClientAddr());
				if (getType() == TYPE::CONFIRM) {
					errMsg->setType(TYPE::ACK);
				}
				else {
					errMsg->setType(TYPE::NON_CONFIRM);
				}
				errMsg->setCode(code);

				if (err.length() > 0) {
					shared_ptr<string> errPayload(new string(err));
					errMsg->addOptionNum(Option::NUMBER::CONTENT_FORMAT, static_cast<size_t>(CONTENT_FORMAT::TEXT_PLAIN));
					errMsg->setPayload(errPayload);
				}

				return errMsg;
			}

			static inline uint16_t getNxtMsgId() {
				static uint16_t nxtMsgId = Helper::genRand16();

				nxtMsgId++;
				if (nxtMsgId == 0) {
					nxtMsgId++;
				}
				return nxtMsgId;
			}

			static inline string getNxtToken() {
				return Helper::generateUniqueToken8();
			}

			bool serializeHeader(string& str) {

				uint8_t verTypeTokLen = (VERSION << 6) |
					(static_cast<uint8_t>(m_type) << 4) | (m_token.length() & 0x0F);
				str.push_back(verTypeTokLen);

				str.push_back(static_cast<uint8_t>(m_code));

				uint16_t mid = htons(m_msgId);
				str.push_back(mid & 0xFF); str.push_back((mid >> 8) & 0xFF);

				str.append(m_token);

				uint16_t prevOptNumb = 0;
				for (auto& [key, opt] : m_optList) {
					opt->serialize(str, prevOptNumb);
					prevOptNumb = static_cast<uint16_t>(opt->getNumber());
				}

				return true;
			}

			bool serialize(string& str) {

				serializeHeader(str);

				if (m_payload != nullptr) {
					str.push_back(PAYLOAD_MARKER);
					str.reserve(str.size() + m_payload->size());
					str.append(*m_payload);
				}

				return true;
			}

			bool deserialize(const string& str, size_t& strIndex) {
				
				uint8_t verTypeTokLen = str[strIndex];
				strIndex++;
				m_type = static_cast<TYPE>((verTypeTokLen >> 4) & 0x03);
				uint8_t tokenLen = verTypeTokLen & 0x0F;

				m_code = static_cast<CODE>(str[strIndex]);
				strIndex++;

				uint16_t mid = (str[strIndex] & 0xFF) | ((str[strIndex + 1] << 8) & 0xFF00);
				strIndex += 2;
				m_msgId = ntohs(mid);

				m_token.clear();
				m_token = str.substr(strIndex, tokenLen);
				strIndex += tokenLen;

				m_optList.clear();

				if (m_type == TYPE::RESET) {
					return true;
				}

				uint16_t prevOptNumb = 0;
				while (strIndex < str.length()) {

					if ((str[strIndex] & PAYLOAD_MARKER) == PAYLOAD_MARKER) {
						strIndex++;
						if (strIndex > str.length()) { // Empty string
							m_payload = make_shared<string>(string());
						}
						else {
							m_payload = std::make_shared<std::string>();
							size_t len = str.size() - strIndex;
							m_payload->reserve(len);
							m_payload->append(str.data() + strIndex, len);
						}

						strIndex = str.length();
						break;
					}

					unique_ptr<Option> newOpt = make_unique<Option>();
					newOpt->deserialize(str, strIndex, prevOptNumb);
					prevOptNumb = static_cast<uint16_t>(newOpt->getNumber());

					m_optList.emplace(newOpt->getNumber(), std::move(newOpt));
				}

				return true;
			}

		private:

			static const uint8_t VERSION = 0x01;

			TYPE m_type = TYPE::CONFIRM;
			CODE m_code = CODE::EMPTY;
			string m_token;
			multimap<Option::NUMBER, unique_ptr<Option>> m_optList;
			uint16_t m_msgId = 0;
			shared_ptr<string> m_payload = nullptr;
		};
	}
}