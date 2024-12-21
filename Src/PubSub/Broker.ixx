//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <unordered_map>
#include <chrono>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>

export module PubSub:Broker;

import Coap;
import Toolbox;
import :CollectionResource;
import :TopicCfgResource;
import :TopicDataResource;
import :Session;
import :BrokerIf;

using namespace std;
using namespace netcoap::coap;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace pubsub {

		export class Broker : public BrokerIf {
		public:
			static const string WELL_KNOWN_CORE;

			Broker(JsonPropTree& cfg, IoSession& io) :
				m_io(io), m_cfg(cfg) {

				registerResourceCb(WELL_KNOWN_CORE, bind(
					&Broker::handleWellKnownCoreCb, this,
					placeholders::_1, placeholders::_2));
			}

			void runSession(Session* sess) {
				lock_guard<mutex> lock(m_threadPoolmtx);
				CoroPoolTask task = sess->run();
				m_threadPool.runTask(std::move(task));
			}

			void msgRcv(shared_ptr<Message> msg) {
				m_msgRcvQ.push(msg);
			}

			bool registerResourceCb(string uri, TopicCfgDataResource::ResourceCb resourceCb) {
				if (m_resourceCbMap.find(uri) != m_resourceCbMap.end()) {
					return false;
				}

				m_resourceCbMap[uri] = resourceCb;
				
				return true;
			}

			void handleWellKnownCoreCb(shared_ptr<Message> req, Session* sess) {

				if (req->getType() != Message::TYPE::CONFIRM) {
					return;
				}

				if (req->getCode() == Message::CODE::OP_GET) {

					string query = req->getOptionRepeatStr(Option::NUMBER::URI_QUERY, Option::DELIM_QUERY);

					if (query.find(TopicCfgDataResource::RT_CORE_PS_COLL) != string::npos) {

						shared_ptr<string> names(new string());
						for (auto& [name, collectionResource] : m_collectionResourceMap) {
							names->append(
								'<' + name + ">;" +
								"rt=" + collectionResource->getResourceType() + ';' +
								"ct=" + to_string(static_cast<int>(Message::CONTENT_FORMAT::APP_LINK_FORMAT)) + ',');
						}

						if (!names->empty() && names->back() == ',') {
							names->pop_back();
						}

						shared_ptr<Message> resp = req->buildAckResponse();

						resp->setCode(Message::CODE::CONTENT);
						resp->addOptionNum(
							Option::NUMBER::CONTENT_FORMAT,
							static_cast<size_t>(Message::CONTENT_FORMAT::APP_LINK_FORMAT));
						resp->setPayload(names);

						sess->sessionSend(resp);
					}
					else if (query.find(TopicCfgDataResource::RT_CORE_PS_CONF) != string::npos) {

						shared_ptr<string> names(new string());
						for (auto& [name, collectionResource] : m_collectionResourceMap) {
							string nameList = collectionResource->getTopicDiscovery(query);
							names->append(nameList + ",");
						}

						if (!names->empty() && names->back() == ',') {
							names->pop_back();
						}

						shared_ptr<Message> resp = req->buildAckResponse();

						resp->setCode(Message::CODE::CONTENT);
						resp->addOptionNum(
							Option::NUMBER::CONTENT_FORMAT,
							static_cast<size_t>(Message::CONTENT_FORMAT::APP_LINK_FORMAT));
						resp->setPayload(names);

						sess->sessionSend(resp);
					}
				}
			};

			void run() {
				
				m_io.init(m_cfg);

				while (true) {

					ServerIoResponse* ioResp = dynamic_cast<ServerIoResponse*>(m_io.getData());
					if (ioResp->statusRet == ServerIoResponse::STATUS_RETURN::NEW_CONNECTION) {

						unique_ptr<Session> sess = make_unique<Session>(
							ioResp->clientAddr,
							m_cfg, m_io,
							this,
							ioResp->inpQ);

						Session* sessPtr = sess.get();
						m_sessionMap[ioResp->clientAddr.toString()] = std::move(sess);
						runSession(sessPtr);
					}

					shared_ptr<Message> reqMsg;
					bool haveReqMsg = m_msgRcvQ.pop(reqMsg);
					if (haveReqMsg) {
						string uriPath = reqMsg->getOptionRepeatStr(Option::NUMBER::URI_PATH, Option::DELIM_PATH);
						string uriQuery = reqMsg->getOptionRepeatStr(Option::NUMBER::URI_QUERY, Option::DELIM_QUERY);
						Session* sess = m_sessionMap[reqMsg->getClientAddr().toString()].get();

						if ((reqMsg->getCode() == Message::CODE::OP_GET) ||
							(reqMsg->getCode() == Message::CODE::OP_PUT) ||
							(reqMsg->getCode() == Message::CODE::OP_FETCH) ||
							(reqMsg->getCode() == Message::CODE::OP_IPATCH)) {

							if (m_resourceCbMap.find(uriPath) == m_resourceCbMap.end()) {
								shared_ptr<Message> errMsg = reqMsg->buildErrResponse(Message::CODE::FORBIDDEN, "");
								sess->sessionSend(errMsg);
								
								continue;
							}

							TopicCfgDataResource::ResourceCb cb = m_resourceCbMap[uriPath];

							cb(reqMsg, sess);
						}
						else if (reqMsg->getCode() == Message::CODE::OP_POST) {

							shared_ptr<string> payLoad = reqMsg->getPayload();
							JsonPropTree jsonPropTree;
							jsonPropTree.fromCborStr(*payLoad);

							string topicName = jsonPropTree.get<string>(TopicCfgResource::TOPIC_NAME);
							string resourceType = jsonPropTree.get<string>(TopicCfgResource::RESOURCE_TYPE);

							string resourceUri = uriPath;
							string topicNameUriPath = resourceUri + Option::DELIM_PATH + Helper::generateUniqueToken8();
							string topicDataUriPath = jsonPropTree.get<string>(TopicCfgResource::TOPIC_DATA);

							CollectionResource* collectionResource = m_collectionResourceMap[resourceUri].get();
							if (collectionResource == nullptr) {
								unique_ptr<CollectionResource> collectResource = make_unique<CollectionResource>();
								collectionResource = collectResource.get();
								m_collectionResourceMap[resourceUri] = std::move(collectResource);
							}

							TopicDataResource* topicData = collectionResource->createTopicData(topicDataUriPath, topicNameUriPath);
							if (!topicData) {
								
								shared_ptr<Message> errMsg = reqMsg->buildErrResponse(Message::CODE::FORBIDDEN, "");
								sess->sessionSend(errMsg);

								continue;
							}
							TopicCfgResource* topicCfg = collectionResource->createTopicCfg(topicNameUriPath);

							registerResourceCb(resourceUri, bind(
								&CollectionResource::handleCb, collectionResource,
								placeholders::_1, placeholders::_2));

							registerResourceCb(topicNameUriPath, bind(
								&TopicCfgResource::handleCb, topicCfg,
								placeholders::_1, placeholders::_2));

							registerResourceCb(topicDataUriPath, bind(
								&TopicDataResource::handleCb, topicData,
								placeholders::_1, placeholders::_2));

							JsonValue* mapVal = jsonPropTree.get<JsonValue*>("");
							if (mapVal != nullptr) {
								for (auto& [key, val] : mapVal->val.mapVal) {
									topicCfg->setProp(key, val.get());
								}
							}

							shared_ptr<Message> respMsg = reqMsg->buildAckResponse();
							respMsg->setCode(Message::CODE::CREATED);
							respMsg->addOptionNum(
								Option::NUMBER::CONTENT_FORMAT,
								static_cast<size_t>(Message::CONTENT_FORMAT::APP_CBOR));
							respMsg->addOptionRepeatStr(
								Option::NUMBER::LOCATION_PATH,
								topicNameUriPath, Option::DELIM_PATH);

							shared_ptr<string> cbor(new string());
							jsonPropTree.toCborStr(*cbor);
							respMsg->setPayload(cbor);

							sess->sessionSend(respMsg);
						}
						else if (reqMsg->getCode() == Message::CODE::OP_DELETE) {

							string uriTopicCfgData = reqMsg->getOptionRepeatStr(Option::NUMBER::URI_PATH, Option::DELIM_PATH);
							bool deleted = false;

							for (auto& [name, val] : m_collectionResourceMap) {

								CollectionResource* collectionResrc = val.get();

								unique_ptr<string> delUriTopicCfgData =
									collectionResrc->deleteTopicCfgData(uriTopicCfgData);

								if (delUriTopicCfgData != nullptr) {
									m_resourceCbMap.erase(uriTopicCfgData);
									m_resourceCbMap.erase(*delUriTopicCfgData);

									deleted = true;

									shared_ptr<Message> respMsg = reqMsg->buildAckResponse();
									respMsg->setCode(Message::CODE::DELETED);

									sess->sessionSend(respMsg);

									break;
								}
							}

							if (!deleted) {
								shared_ptr<Message> errMsg = reqMsg->buildErrResponse(Message::CODE::NOT_FOUND, "");
								sess->sessionSend(errMsg);
							}
						}
						else {
							shared_ptr<Message> errMsg = reqMsg->buildErrResponse(Message::CODE::NOT_IMPLEMENTED, "");
							sess->sessionSend(errMsg);
						}
					}

					m_io.outDataToNet();
				}
			}

		private:

			IoSession& m_io;
			JsonPropTree& m_cfg;
			unordered_map<string, unique_ptr<CollectionResource>> m_collectionResourceMap;
			unordered_map<string, TopicCfgDataResource::ResourceCb> m_resourceCbMap;
			unordered_map<string, unique_ptr<Session>> m_sessionMap;
			SyncQ<shared_ptr<Message>> m_msgRcvQ;
			mutex m_threadPoolmtx;
			CoroPool m_threadPool{ (int)thread::hardware_concurrency() };
		};

		const string Broker::WELL_KNOWN_CORE = "/well-known/core";
	}
}
