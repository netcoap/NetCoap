//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <string>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <memory>
#include <ctime>

#include "Toolbox/Toolbox.h"

export module PubSub:TopicCfgResource;

import Coap;
import Toolbox;
import :Session;

using namespace std;
using namespace chrono;

using namespace netcoap::coap;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace pubsub {

		export class TopicCfgDataResource {
		public:

			static const string RT_CORE_PS;
			static const string RT_CORE_PS_COLL;
			static const string RT_CORE_PS_CONF;
			static const string RT_CORE_PS_DATA;

			virtual ~TopicCfgDataResource() {}

			using ResourceCb = function<void(shared_ptr<Message> req, Session* sess)>;

			virtual void handleCb(shared_ptr<Message> req, Session* sess) = 0;

			virtual string getResourceType() const = 0;

			ResourceCb getHandler() {
				return [this](shared_ptr<Message> req, Session* sess)
					{ this->handleCb(req, sess); };
			}
		};

		const string TopicCfgDataResource::RT_CORE_PS = "core.ps";
		const string TopicCfgDataResource::RT_CORE_PS_COLL = "core.ps.coll";
		const string TopicCfgDataResource::RT_CORE_PS_CONF = "core.ps.conf";
		const string TopicCfgDataResource::RT_CORE_PS_DATA = "core.ps.data";

		export class TopicCfgVal {
		public:

			enum class VALUE_TYPE { UNDEFINED, DATETIME_VAL, STRING_VAL, INT_VAL, BOOL_VAL };

			typedef union Value {

				Value() {}
				~Value() {}

				string strVal;
				int intVal;
				time_point<system_clock> tpVal;
				bool boolVal;

			} Value;

			~TopicCfgVal() {
				if (vtype == VALUE_TYPE::STRING_VAL) {
					val.strVal.~string();
				}

				if (vtype == VALUE_TYPE::DATETIME_VAL) {
					val.tpVal.~time_point<system_clock>();
				}
			}

			TopicCfgVal() {
			}

			TopicCfgVal(TopicCfgVal& val) {
				*this = val;
			}

			string toJsonString() {

				switch (vtype) {
				case VALUE_TYPE::STRING_VAL:
					return "\"" + val.strVal + "\"";
					break;

				case VALUE_TYPE::BOOL_VAL:
					if (val.boolVal) {
						return "true";
					}
					else {
						return "false";
					}
					break;

				case VALUE_TYPE::DATETIME_VAL: {
					time_t time = system_clock::to_time_t(val.tpVal);
					struct tm utc_tm;
					#ifdef _WIN32
					_gmtime64_s(&utc_tm, &time);
					#else
					gmtime_r(&time, &utc_tm);
					#endif
					char dateTimeStr[50];
					strftime(dateTimeStr, sizeof(dateTimeStr), "\"%Y-%m-%dT%H:%M:%SZ\"", &utc_tm);

					return std::string(dateTimeStr);
				}
				break;

				case VALUE_TYPE::INT_VAL:
						return to_string(val.intVal);
				break;

				default:
					return "";
					break;
				}

			}

			TopicCfgVal& operator=(TopicCfgVal& rhs) {
				if (this == &rhs) {
					return *this;
				}

				vtype = rhs.vtype;
				switch (rhs.vtype) {
					case VALUE_TYPE::STRING_VAL:
						*this = rhs.val.strVal;
					break;

					case VALUE_TYPE::BOOL_VAL:
						val.boolVal = rhs.val.boolVal;
					break;

					case VALUE_TYPE::INT_VAL:
						val.intVal = rhs.val.intVal;
					break;

					case VALUE_TYPE::DATETIME_VAL:
						val.tpVal = rhs.val.tpVal;
					break;

					default:
					break;
				}

				return *this;
			}

			TopicCfgVal& operator=(const time_point<system_clock> rhs) {
				new (&val.tpVal) time_point<system_clock>();
				vtype = VALUE_TYPE::DATETIME_VAL;
				val.tpVal = rhs;

				return *this;
			}

			TopicCfgVal& operator=(const string rhs) {
				new (&val.strVal) string();
				vtype = VALUE_TYPE::STRING_VAL;
				val.strVal = rhs;

				return *this;
			}

			TopicCfgVal& operator=(const int rhs) {
				vtype = VALUE_TYPE::INT_VAL;
				val.intVal = rhs;

				return *this;
			}

			TopicCfgVal& operator=(const bool rhs) {
				vtype = VALUE_TYPE::BOOL_VAL;
				val.boolVal = rhs;

				return *this;
			}

			VALUE_TYPE vtype = VALUE_TYPE::UNDEFINED;
			Value val;
		};

		export class TopicCfgResource : public TopicCfgDataResource {
		public:

			static const string TOPIC_NAME;
			static const string TOPIC_DATA;
			static const string RESOURCE_TYPE;
			static const string TOPIC_MEDIA_TYPE;
			static const string TOPIC_TYPE;
			static const string EXPIRATION_DATE;
			static const string MAX_SUBSCRIBERS;
			static const string OBSERVER_CHECK;
			static const string TOPIC_HISTORY;
			static const string INITIALIZE;

			static const string CONFIG_FILTER;

			TopicCfgResource() {

				time_point<system_clock> nowPlus100year = system_clock::now() +
					hours(static_cast < int>(100 * 365.25 * 24));

				m_topicCfgMap[TOPIC_NAME] = string("");
				m_topicCfgMap[TOPIC_DATA] = string("");
				m_topicCfgMap[RESOURCE_TYPE] = TopicCfgDataResource::RT_CORE_PS_CONF;
				m_topicCfgMap[TOPIC_MEDIA_TYPE] = static_cast<int>(Message::CONTENT_FORMAT::TEXT_PLAIN);
				m_topicCfgMap[TOPIC_TYPE] = string("");
				m_topicCfgMap[EXPIRATION_DATE] = nowPlus100year;
				m_topicCfgMap[MAX_SUBSCRIBERS] = 1000;
				m_topicCfgMap[OBSERVER_CHECK] = 86400;
				m_topicCfgMap[TOPIC_HISTORY] = 0;
				m_topicCfgMap[INITIALIZE] = false;

				TopicCfgVal mediaType = m_topicCfgMap[TOPIC_MEDIA_TYPE];
			}

			TopicCfgVal getProp(string key) {

				TopicCfgVal val;
				val = m_topicCfgMap[key];

				return val;
			}

			bool setProp(string key, JsonValue* val) {

				if (val == nullptr) {
					return false;
				}

				TopicCfgVal propVal;

				switch (val->valType) {

					case JsonValue::VALUE_TYPE::INT_VAL:
						propVal = val->val.intVal;
						m_topicCfgMap[key] = propVal;
					break;

					case JsonValue::VALUE_TYPE::STRING_VAL: {

						if (key.find(EXPIRATION_DATE) != string::npos) {
							istringstream ss(val->val.strVal);
							time_point<std::chrono::system_clock> tp;

							#ifdef _WIN32
							ss >> parse("%Y-%m-%dT%H:%M:%SZ", tp);
							#else
							tm tm = {};
							ss >> get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
							time_t time = timegm(&tm);
    						tp = system_clock::from_time_t(time);
							#endif

							propVal = tp;
						}
						else {
							propVal = val->val.strVal;
						}

						m_topicCfgMap[key] = propVal;
					}
					break;

					case JsonValue::VALUE_TYPE::BOOL_VAL:
						propVal = val->val.boolVal;
						m_topicCfgMap[key] = propVal;
					break;

					default:
						LIB_MSG_ERR_THROW_EX(
							"TopicCfgResource.setProp do not support type {}",
							static_cast<int>(val->valType));
						break;
				}

				return true;
			}

			bool isPropertiesEq(JsonPropTree& prop) {
				
				JsonValue* v = prop.get<JsonValue*>("");

				if ((v == nullptr) || (v->valType != JsonValue::VALUE_TYPE::MAP_VAL)) {
					return false;
				}

				for (auto& [key, val] : v->val.mapVal) {
					TopicCfgVal lhs = m_topicCfgMap[key];
					
					switch (lhs.vtype) {

						case TopicCfgVal::VALUE_TYPE::INT_VAL:
							if (lhs.val.intVal != val->val.intVal) {
								return false;
							}
						break;

						case TopicCfgVal::VALUE_TYPE::STRING_VAL:
							if (lhs.val.strVal != val->val.strVal) {
								return false;
							}
						break;

						default:
							LIB_MSG_ERR_THROW_EX(
								"TopicCfgResource.isPropertiesEq type {} compare not supported",
								static_cast<int>(lhs.vtype));
						break;
					}
				}

				return true;
			}

			string getResourceType() const {
				return TopicCfgDataResource::RT_CORE_PS_CONF;
			}

			inline Message::CONTENT_FORMAT getMediaType() {
				TopicCfgVal val = m_topicCfgMap[TOPIC_MEDIA_TYPE];

				return static_cast<Message::CONTENT_FORMAT>(val.val.intVal);
			}

			void handleCb(shared_ptr<Message> req, Session* sess) {

				if (req->getCode() == Message::CODE::OP_GET) {

					string json = "{";
					for (auto& [key, val] : m_topicCfgMap) {
							
						if (key.find(INITIALIZE) != string::npos) {
							continue;
						}
						else if (key.find(OBSERVER_CHECK) != string::npos) {
							continue;
						}

						string valJsonStr = val.toJsonString();
						json.append(
							"\"" + key + "\"" + ':' + valJsonStr + ',');

					}

					if (!json.empty() && json.back() == ',') {
						json.pop_back();
					}

					json.append("}");

					JsonPropTree propTree;
					propTree.fromJsonStr(json);
					shared_ptr<string> payload(new string());
					propTree.toCborStr(*payload);

					shared_ptr<Message> resp = req->buildAckResponse();

					resp->setCode(Message::CODE::CONTENT);
					resp->addOptionNum(
						Option::NUMBER::CONTENT_FORMAT,
						static_cast<size_t>(Message::CONTENT_FORMAT::APP_CBOR));

					resp->setPayload(payload);

					sess->sessionSend(resp);
				}
				else if (req->getCode() == Message::CODE::OP_FETCH) {

					JsonPropTree propTree;
					propTree.fromCborStr(*req->getPayload());

					JsonValue* prop = propTree.get<JsonValue*>(CONFIG_FILTER);
					string json = "{";
					if (prop != nullptr) {
						for (int i = 0; i < prop->val.listVal.size(); i++) {
							JsonValue* key = prop->val.listVal[i].get();

							if (key->val.strVal.find(INITIALIZE) != string::npos) {
								continue;
							}
							else if (key->val.strVal.find(OBSERVER_CHECK) != string::npos) {
								continue;
							}

							TopicCfgVal cfgVal = m_topicCfgMap[key->val.strVal];
							string valJsonStr = cfgVal.toJsonString();

							json.append(
								"\"" + key->val.strVal + "\"" + ':' + valJsonStr + ','
							);
						}
					}

					if (!json.empty() && json.back() == ',') {
						json.pop_back();
					}

					json.append("}");

					shared_ptr<string> payload(new string());
					propTree.fromJsonStr(json);
					propTree.toCborStr(*payload);

					shared_ptr<Message> resp = req->buildAckResponse();

					resp->setCode(Message::CODE::CONTENT);
					resp->addOptionNum(
						Option::NUMBER::CONTENT_FORMAT,
						static_cast<size_t>(Message::CONTENT_FORMAT::APP_CBOR));

					resp->setPayload(payload);

					sess->sessionSend(resp);
				}
				else if ((req->getCode() == Message::CODE::OP_PUT) ||
						(req->getCode() == Message::CODE::OP_IPATCH)) {

					JsonPropTree propTree;
					propTree.fromCborStr(*req->getPayload());

					JsonValue* jsonVal = propTree.get<JsonValue*>("");
					for (auto& [key, val] : jsonVal->val.mapVal) {

						if (key.find(TOPIC_DATA) != string::npos) { // Cannot modify data
							continue;
						}

						if (key.find(INITIALIZE) != string::npos) {
							continue;
						}
						else if (key.find(OBSERVER_CHECK) != string::npos) {
							continue;
						}

						setProp(key, val.get());
					}

					string json = "{";
					for (auto& [key, val] : m_topicCfgMap) {

						if (key.find(INITIALIZE) != string::npos) {
							continue;
						}
						else if (key.find(OBSERVER_CHECK) != string::npos) {
							continue;
						}

						json.append(
							"\"" + key + "\"" + ':' + val.toJsonString() + ','
						);
					}

					if (!json.empty() && json.back() == ',') {
						json.pop_back();
					}

					json.append("}");

					shared_ptr<string> payload(new string());
					propTree.fromJsonStr(json);
					propTree.toCborStr(*payload);

					shared_ptr<Message> resp = req->buildAckResponse();

					resp->setCode(Message::CODE::CONTENT);
					resp->addOptionNum(
						Option::NUMBER::CONTENT_FORMAT,
						static_cast<size_t>(Message::CONTENT_FORMAT::APP_CBOR));

					resp->setPayload(payload);

					sess->sessionSend(resp);
				}
				else {
					shared_ptr<Message> errMsg = req->buildErrResponse(Message::CODE::NOT_IMPLEMENTED, "");
					sess->sessionSend(errMsg);
				}
			}

		private:
			unordered_map<string, TopicCfgVal> m_topicCfgMap;
		};

		const string TopicCfgResource::TOPIC_NAME = "topic-name";
		const string TopicCfgResource::TOPIC_DATA = "topic-data";
		const string TopicCfgResource::RESOURCE_TYPE = "resource-type";
		const string TopicCfgResource::TOPIC_MEDIA_TYPE = "topic-media-type";
		const string TopicCfgResource::TOPIC_TYPE = "topic-type";
		const string TopicCfgResource::EXPIRATION_DATE = "expiration-date";
		const string TopicCfgResource::MAX_SUBSCRIBERS = "max-subscribers";
		const string TopicCfgResource::OBSERVER_CHECK = "observer-check";
		const string TopicCfgResource::TOPIC_HISTORY = "topic-history";
		const string TopicCfgResource::INITIALIZE = "intialize";

		const string TopicCfgResource::CONFIG_FILTER = "conf-filter";
	}
}