//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>

#include "Toolbox/Toolbox.h"
#include "Coap/Coap.h"

export module Coap:Block1Xfer;

import Toolbox;
import :Message;
import :Option;

using namespace std;

namespace netcoap {
	namespace coap {

		export class Block1Xfer {
		public:

			shared_ptr<Message> xfer(shared_ptr<Message> msg) {

				if (msg->getPayload()) {

					if (msg->getPayload()->length() > MAX_BLOCK_SIZE) {
						return clientBlockXfer(msg);
					}
				}

				return msg;
			}

			shared_ptr<Message> rcv(shared_ptr<Message> msg) {

				if (msg->isOptionNumExist(Option::NUMBER::BLOCK1)) {

					BlockData blockData = m_tokenBlockMap[msg->getToken()];
					if (blockData.block.getMore()) {
						return blockData.msg;
					}
					else {
						m_tokenBlockMap.erase(msg->getToken());
						return nullptr;
					}
				}

				return nullptr;
			}

		private:

			class BlockData {
			public:
				shared_ptr<Message> msg = nullptr;
				Block block;
				string uriPath;
				string uriQuery;

				shared_ptr<string> rcvBuf = nullptr;
			};

			shared_ptr<Message> clientBlockXfer(shared_ptr<Message> msg) {

				BlockData blockData;
				if (m_tokenBlockMap.find(msg->getToken()) != m_tokenBlockMap.end()) {
					blockData = m_tokenBlockMap[msg->getToken()];
					blockData.block.incrNum();
				}
				else {
					blockData.msg = msg;
					blockData.uriPath = msg->getOptionRepeatStr(Option::NUMBER::URI_PATH, Option::DELIM_PATH);
					blockData.uriQuery = msg->getOptionRepeatStr(Option::NUMBER::URI_QUERY, Option::DELIM_QUERY);
					blockData.block.setSz(BLOCK_SZX);
				}

				shared_ptr<Message> req;

				size_t xmitLen = MAX_BLOCK_SIZE;
				size_t nxtXferIdx = blockData.block.getNum() << (blockData.block.getSz() + 4);
				if ((nxtXferIdx + xmitLen) >= blockData.msg->getPayload()->length()) {
					blockData.block.setMore(0);
					req = blockData.msg;
					req->setMsgId(Message::getNxtMsgId());
					xmitLen = blockData.msg->getPayload()->length() - nxtXferIdx;
				}
				else {
					req = make_shared<Message>();
					req->setMsgId(Message::getNxtMsgId());
					req->setType(Message::TYPE::CONFIRM);
					req->setToken(blockData.msg->getToken());
					req->setCode(msg->getCode());
					blockData.block.setMore(1);
					req->addOptionRepeatStr(Option::NUMBER::URI_PATH, blockData.uriPath, Option::DELIM_PATH);
					req->addOptionRepeatStr(Option::NUMBER::URI_QUERY, blockData.uriQuery, Option::DELIM_QUERY);
				}

				m_tokenBlockMap[req->getToken()] = blockData;

				req->addOptionBlock(Option::NUMBER::BLOCK1, blockData.block);

				shared_ptr<string> payload(new string());
				payload->reserve(payload->size() + xmitLen);
				payload->append(blockData.msg->getPayload()->data() + nxtXferIdx, xmitLen);
				req->setPayload(payload);

				return req;
			}

			unordered_map<string, BlockData> m_tokenBlockMap;
		};
	}
}