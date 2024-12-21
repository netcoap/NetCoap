//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

export module PubSub:CollectionResource;

import Coap;
import Toolbox;
import :Session;
import :TopicCfgResource;
import :TopicDataResource;

using namespace std;
using namespace netcoap::coap;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace pubsub {

		export class CollectionResource {
		public:

			void handleCb(shared_ptr<Message> req, Session* sess) {

				if (req->getCode() == Message::CODE::OP_GET) {

					string query = req->getOptionRepeatStr(Option::NUMBER::URI_QUERY, Option::DELIM_QUERY);

					if (query.length() == 0) { // Retrieve all topic
						shared_ptr<string> names(new string(getTopicDiscovery(TopicCfgDataResource::RT_CORE_PS_CONF)));

						shared_ptr<Message> resp = req->buildAckResponse();

						resp->setCode(Message::CODE::CONTENT);
						resp->addOptionNum(
							Option::NUMBER::CONTENT_FORMAT,
							static_cast<size_t>(Message::CONTENT_FORMAT::APP_LINK_FORMAT));
						resp->setPayload(names);

						sess->sessionSend(resp);
					}
					else if (query.find(TopicCfgDataResource::RT_CORE_PS_DATA) != string::npos) {

						shared_ptr<string> names(new string(getTopicDiscovery(query)));

						shared_ptr<Message> resp = req->buildAckResponse();

						resp->setCode(Message::CODE::CONTENT);
						resp->addOptionNum(
							Option::NUMBER::CONTENT_FORMAT,
							static_cast<size_t>(Message::CONTENT_FORMAT::APP_LINK_FORMAT));
						resp->setPayload(names);

						sess->sessionSend(resp);
					}
					else {
						shared_ptr<Message> errMsg = req->buildErrResponse(Message::CODE::NOT_IMPLEMENTED, "");
						sess->sessionSend(errMsg);
					}
				}
				else if (req->getCode() == Message::CODE::OP_FETCH) {

					JsonPropTree jsonPropTree;
					jsonPropTree.fromCborStr(*req->getPayload());

					shared_ptr<string> names(new string());
					for (auto& [name, cfgDataResrc] : m_topicCfgDataResourceMap) {

						if (TopicCfgDataResource::RT_CORE_PS_CONF.find(cfgDataResrc->getResourceType()) != string::npos) {
							TopicCfgResource* cfgResrc = dynamic_cast<TopicCfgResource*>(cfgDataResrc.get());
							
							if (cfgResrc->isPropertiesEq(jsonPropTree)) {
								names->append(
									"<" + name + ">; rt=" +
									TopicCfgDataResource::RT_CORE_PS_CONF + ";" +
									"ct=" + to_string(static_cast<int>(cfgResrc->getMediaType())) + ",");
							}
						}
					}

					if (!names->empty() && names->back() == ',') {
						names->erase(names->size() - 1);
					}

					shared_ptr<Message> resp = req->buildAckResponse();

					resp->setCode(Message::CODE::CONTENT);
					resp->addOptionNum(
						Option::NUMBER::CONTENT_FORMAT,
						static_cast<size_t>(Message::CONTENT_FORMAT::APP_LINK_FORMAT));
					resp->setPayload(names);

					sess->sessionSend(resp);
				}
				else {
					shared_ptr<Message> errMsg = req->buildErrResponse(Message::CODE::NOT_IMPLEMENTED, "");
					sess->sessionSend(errMsg);
				}
			};

			TopicCfgResource* createTopicCfg(string uri) {
				TopicCfgResource* retTopicCfg = nullptr;
				
				if (m_topicCfgDataResourceMap.find(uri) == m_topicCfgDataResourceMap.end()) {
					unique_ptr<TopicCfgResource> topicCfg = make_unique<TopicCfgResource>();
					retTopicCfg = topicCfg.get();
					m_topicCfgDataResourceMap.emplace(uri,
						unique_ptr<TopicCfgDataResource>(std::move(topicCfg)));

				}

				return retTopicCfg;
			}

			TopicDataResource* createTopicData(string uri, string uriTopicCfgPath) {
				TopicDataResource* retTopicData = nullptr;

				if (m_topicCfgDataResourceMap.find(uri) == m_topicCfgDataResourceMap.end()) {
					unique_ptr<TopicDataResource> topicData = make_unique<TopicDataResource>(uriTopicCfgPath);
					retTopicData = topicData.get();
					m_topicCfgDataResourceMap[uri] = std::move(topicData);
				}

				return retTopicData;
			}

			unique_ptr<string> deleteTopicCfgData(string uriTopicCfgData) {

				unique_ptr<string> retDelTopicCfgData = nullptr;

				if (m_topicCfgDataResourceMap.find(uriTopicCfgData) != m_topicCfgDataResourceMap.end()) {

					TopicCfgDataResource* topicCfgDataResrc = m_topicCfgDataResourceMap[uriTopicCfgData].get();

					TopicCfgResource* topicCfgResrc = dynamic_cast<TopicCfgResource*>(topicCfgDataResrc);
					if (topicCfgResrc) {

						TopicCfgVal uriTopicData = topicCfgResrc->getProp(TopicCfgResource::TOPIC_DATA);

						m_topicCfgDataResourceMap.erase(uriTopicCfgData);
						m_topicCfgDataResourceMap.erase(uriTopicData.val.strVal);

						retDelTopicCfgData = make_unique<string>(uriTopicData.val.strVal);

						return retDelTopicCfgData;

					}
					else { // Data resource

						TopicDataResource* topicDataResrc = dynamic_cast<TopicDataResource*>(topicCfgDataResrc);
						string uriTopicCfgPath = topicDataResrc->getUriTopicCfgPath();

						TopicCfgDataResource* topicCfgDataResrc = m_topicCfgDataResourceMap[uriTopicCfgPath].get();
						TopicCfgResource* topicCfgResrc = dynamic_cast<TopicCfgResource*>(topicCfgDataResrc);

						JsonValue uriTopicCfgVal;
						uriTopicCfgVal.newVal<string>();
						uriTopicCfgVal.val.strVal = "";
						topicCfgResrc->setProp(TopicCfgResource::TOPIC_DATA, &uriTopicCfgVal);

						m_topicCfgDataResourceMap.erase(uriTopicCfgData);

						retDelTopicCfgData = make_unique<string>(uriTopicCfgData);

						return retDelTopicCfgData;
					}
				}

				return retDelTopicCfgData;
			}

			TopicCfgDataResource::ResourceCb getTopicCfgDataHandler(string uri) {
				return m_topicCfgDataResourceMap[uri]->getHandler();
			}

			string getResourceType() const {
				return TopicCfgDataResource::RT_CORE_PS_COLL;
			}

			string getTopicDiscovery(string query) {
				string names;

				for (auto& [name, cfgDataResrc] : m_topicCfgDataResourceMap) {

					if (query.find(cfgDataResrc->getResourceType()) != string::npos) {
						names.append(
							'<' + name + ">;" +
							"rt=" + cfgDataResrc->getResourceType() + ';');

						if (TopicCfgDataResource::RT_CORE_PS_CONF.find(cfgDataResrc->getResourceType()) != string::npos) {
							TopicCfgResource* cfgResrc = dynamic_cast<TopicCfgResource*>(cfgDataResrc.get());
							names.append("ct=" + to_string(static_cast<int>(cfgResrc->getMediaType())) + ',');
						}
						else { // Data
							names.append("obs,");
						}
					}
				}

				if (!names.empty() && names.back() == ',') {
					names.erase(names.size() - 1);
				}

				return names;
			}

		private:

			unordered_map<string, unique_ptr<TopicCfgDataResource>> m_topicCfgDataResourceMap;
		};
	}
}