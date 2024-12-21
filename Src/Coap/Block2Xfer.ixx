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
#include <mutex>

#include "Toolbox/Toolbox.h"
#include "Coap/Coap.h"

export module Coap:Block2Xfer;

import Toolbox;
import :Message;
import :Option;

using namespace std;

namespace netcoap {
	namespace coap {

		export class Block2Xfer {
		public:

			void saveReq(shared_ptr<Message> msg) {
				m_tokenMsgMap[msg->getToken()] = msg;
			}

			shared_ptr<Message> serverXfer(shared_ptr<Message> msg) {

				if (msg->getPayload()) {

					if (msg->getPayload()->size() > MAX_BLOCK_BYTES_XFER) {
						LIB_MSG_ERR_THROW_EX("Error block2 too large");
					}

					if (msg->getPayload()->length() > MAX_BLOCK_SIZE) {
						return serverBlockXfer(msg);
					}
				}

				return msg;
			}

			shared_ptr<Message> serverRcv(shared_ptr<Message> msg) {

				if (!msg->isOptionNumExist(Option::NUMBER::BLOCK2)) {
					return nullptr;
				}

				return serverBlockXfer(msg);
			}

			shared_ptr<Message> clientRcv(shared_ptr<Message> msg, bool &cont) {

				cont = true;

				if (!msg->isOptionNumExist(Option::NUMBER::BLOCK2)) {
					m_tokenMsgMap.erase(msg->getToken());
					
					return nullptr;
				}

				Block rcvBlock = msg->getOptionBlock(Option::NUMBER::BLOCK2);

				BlockData blockData;
				if (m_tokenBlockMap.find(msg->getToken()) != m_tokenBlockMap.end()) {
					blockData = m_tokenBlockMap[msg->getToken()];
					blockData.rcvBuf->reserve(blockData.rcvBuf->size() + msg->getPayload()->size());
					blockData.rcvBuf->append(*msg->getPayload());
				}
				else {
					blockData.rcvBuf = msg->getPayload();
					m_tokenBlockMap[msg->getToken()] = blockData;
				}

				if (!rcvBlock.getMore()) {
					
					msg->setPayload(blockData.rcvBuf);
					m_tokenBlockMap.erase(msg->getToken());

					m_tokenMsgMap.erase(msg->getToken());

				} else if (!msg->isOptionNumExist(Option::NUMBER::OBSERVE)) {
					Block block = msg->getOptionBlock(Option::NUMBER::BLOCK2);

					shared_ptr<Message> req = m_tokenMsgMap[msg->getToken()];
					string uriPath = req->getOptionRepeatStr(Option::NUMBER::URI_PATH, Option::DELIM_PATH);

					shared_ptr<Message> newReq(new Message());
					newReq->setType(Message::TYPE::CONFIRM);
					newReq->setCode(req->getCode());
					newReq->setToken(req->getToken());
					newReq->addOptionRepeatStr(Option::NUMBER::URI_PATH, uriPath, Option::DELIM_PATH);
					newReq->addOptionBlock(Option::NUMBER::BLOCK2, block);

					return newReq;
				}
				else { // More blocks to receive
					cont = false;
				}

				return nullptr;
			}

		private:

			class BlockData {
			public:
				shared_ptr<Message> msg = nullptr;
				Block block;
				string uriPath;

				shared_ptr<string> rcvBuf = nullptr;
			};

			shared_ptr<Message> serverBlockXfer(shared_ptr<Message> msg) {

				BlockData blockData;
				if (m_tokenBlockMap.find(msg->getToken()) != m_tokenBlockMap.end()) {
					blockData = m_tokenBlockMap[msg->getToken()];
					blockData.block.incrNum();
				}
				else {
					blockData.msg = msg;
					blockData.uriPath = msg->getOptionRepeatStr(Option::NUMBER::URI_PATH, Option::DELIM_PATH);;
					blockData.block.setSz(BLOCK_SZX);
				}

				shared_ptr<Message> ack = msg->buildAckResponse();
				ack->setCode(Message::CODE::CONTENT);
				ack->setClientAddr(msg->getClientAddr());

				if (msg->isOptionNumExist(Option::NUMBER::OBSERVE)) {
					uint32_t obsSeq = msg->getOptionNum(Option::NUMBER::OBSERVE);
					ack->addOptionNum(Option::NUMBER::OBSERVE, obsSeq);
				}

				size_t xmitLen = MAX_BLOCK_SIZE;
				size_t nxtXferIdx = blockData.block.getNum() << (blockData.block.getSz() + 4);
				if ((nxtXferIdx + xmitLen) >= blockData.msg->getPayload()->length()) {
					blockData.block.setMore(0);
					xmitLen = blockData.msg->getPayload()->length() - nxtXferIdx;
				}
				else {
					blockData.block.setMore(1);
				}

				m_tokenBlockMap[ack->getToken()] = blockData;

				ack->addOptionBlock(Option::NUMBER::BLOCK2, blockData.block);

				shared_ptr<string> payload(new string());
				payload->reserve(payload->size() + xmitLen);
				payload->append(blockData.msg->getPayload()->data() + nxtXferIdx, xmitLen);
				ack->setPayload(payload);

				return ack;
			}

			unordered_map<string, BlockData> m_tokenBlockMap;
			unordered_map<string, shared_ptr<Message>> m_tokenMsgMap;
		};
	}
}