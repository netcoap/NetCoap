//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <iostream>
#include <string>
#include <memory>

#include "Coap/Coap.h"
#include "Toolbox/Toolbox.h"

export module Coap:Option;

import Toolbox;

using namespace std;

using namespace netcoap::toolbox;

namespace netcoap {
	namespace coap {

		export class OptionSerialize {
		public:
			virtual bool serialize(string& str, size_t optData) {
				return false;
			}
			virtual bool deserialize(const string& str, size_t& strIndex, size_t optData) {
				return false;
			}
		};

		export class OptValue : public OptionSerialize {
		public:

			OptValue(uint16_t minSiz, uint16_t maxSiz) {
				setSiz(minSiz, maxSiz);
			}

			static bool writeOptDeltaOrLen(string &str, uint16_t &optDeltaOrLen) {

				if (optDeltaOrLen > 268) { // 2 bytes delta extended
					optDeltaOrLen -= 269;
					optDeltaOrLen = htons(optDeltaOrLen);
					str.push_back(optDeltaOrLen & 0xFF);
					str.push_back((optDeltaOrLen >> 8) & 0xFF);

					optDeltaOrLen = 14;
				}
				else if (optDeltaOrLen > 12) { // 1 byte delta extended
					optDeltaOrLen -= 13;
					str.push_back(optDeltaOrLen & 0xFF);

					optDeltaOrLen = 13;
				}

				return true;
			}

			static bool readOptDeltaOrLen(const string &str, size_t &strIndex, uint16_t &optDeltaOrLen) {
				
				switch (optDeltaOrLen) {
					case 13:
						optDeltaOrLen = str[strIndex] & 0xFF;
						strIndex++;
						optDeltaOrLen += 13;
						break;

					case 14:
						optDeltaOrLen = (str[strIndex] | (str[strIndex + 1] << 8)) & 0xFFFF;
						optDeltaOrLen = ntohs(optDeltaOrLen);
						strIndex += 2;
						optDeltaOrLen += 269;
						break;

					default:
						break;
				}

				return true;
			}

		protected:

			void setSiz(uint16_t minSiz, uint16_t maxSiz) {
				m_minSiz = minSiz;
				m_maxSiz = maxSiz;
			}

			inline uint16_t getMinSiz() const {
				return m_minSiz;
			}

			inline uint16_t getMaxSiz() const {
				return m_maxSiz;
			}

		private:

			uint16_t m_minSiz = 0;
			uint16_t m_maxSiz = 0;
		};

		export template <typename T>
			class OptVal : public OptValue {};

		export class Block {
		public:

			enum class TYPE { BLOCK1, BLOCK2 };

			inline uint32_t getNum() {
				return m_num;
			}

			inline void setNum(uint32_t num) {
				m_num = num;
			}

			inline void incrNum() {
				m_num++;
			}

			inline uint8_t getMore() {
				return m_more;
			}

			inline void setMore(uint8_t more) {
				m_more = more;
			}

			inline uint8_t getSz() {
				return m_sz;
			}

			inline void setSz(uint8_t sz) {
				m_sz = sz;
			}

		private:
			uint32_t m_num = 0;
			uint8_t m_more = 0;
			uint8_t m_sz = 0;
		};

		export template<>
			class OptVal<Block> : public OptValue {
			public:

				OptVal(uint16_t minSiz, uint16_t maxSiz) : OptValue(minSiz, maxSiz) {
				}

				inline Block get() {
					return m_block;
				}

				inline void set(Block block) {
					m_block = block;
				}

				bool serialize(string& str, size_t optData) {

					size_t block = (m_block.getNum() << 4) | (m_block.getMore() << 3) | m_block.getSz();

					uint16_t len = 1;
					if (block & 0xFF0000) {
						len = 3;
					}
					else if (block & 0xFF00) {
						len = 2;
					}
					uint16_t llen = len;

					OptValue::writeOptDeltaOrLen(str, len);
					str[optData] |= (len & 0x0F);
					
					switch (llen) {

						case 1:
							str.push_back(block);
						break;

						case 2:
							block = htons(block);
							str.push_back((block >> 8) & 0xFF);
							str.push_back(block & 0xFF);
						break;

						default:
							block &= 0xFFFFFF;
							str.push_back((block >> 16) & 0xFF);
							str.push_back((block >> 8) & 0xFF);
							str.push_back(block & 0xFF);							
						break;
					}

					return true;
				}

				bool deserialize(const string& str, size_t& strIndex, size_t optData) {

					uint16_t len = str[optData] & 0x0F;

					OptValue::readOptDeltaOrLen(str, strIndex, len);

					uint32_t num;
					switch (len) {

						case 1:
							num = str[strIndex] & 0xFF;
							strIndex++;
						break;

						case 2:
							num = (str[strIndex] << 8) & 0xFF00;
							num |= (str[strIndex + 1] & 0xFF);
							num &= 0xFFFF;
							strIndex += 2;
							num = ntohs(num);
						break;

						case 3:
							num = (str[strIndex] << 16) & 0xFF0000;
							num |= ((str[strIndex + 1] << 8) & 0xFF00);
							num |= (str[strIndex + 2] & 0xFF);
							strIndex += 3;			
						break;

						default:
							LIB_MSG_ERR("OptVal<Block> deserialize Un-support maxSiz: {}", len);
							return false;
						break;
					}

					m_block.setMore((num >> 3) & 1);
					m_block.setSz(num & 7);
					m_block.setNum(num >> 4);

					return true;

				}

			private:
				Block m_block;
		};

		export template<>
			class OptVal<size_t> : public OptValue {
			public:

				OptVal(uint16_t minSiz, uint16_t maxSiz) : OptValue(minSiz, maxSiz) {
				}

				inline uint32_t get() {
					return m_ival;
				}

				inline void set(uint32_t ival) {
					m_ival = ival;
				}

				bool serialize(string& str, size_t optData) {

					uint16_t len = 1;
					if (m_ival & 0xFF000000) {
						len = 4;
					}
					else if (m_ival & 0xFF0000) {
						len = 3;
					}
					else if (m_ival & 0xFF00) {
						len = 2;
					}

					uint16_t llen = len;

					OptValue::writeOptDeltaOrLen(str, len);
					str[optData] |= (len & 0x0F);

					size_t idx = str.size();

					size_t nval;
					switch (llen) {

						case 1:
							str.push_back(m_ival & 0xFF);
						break;

						case 2:
							nval = htons(m_ival);
							str.push_back((nval >> 8) & 0xFF);
							str.push_back(nval & 0xFF);
						break;

						case 3:
							str.push_back((m_ival >> 16) & 0xFF);
							str.push_back((m_ival >> 8) & 0xFF);
							str.push_back(m_ival & 0xFF);							
						break;

						default:
							nval = htonl(m_ival);
							str.push_back((nval >> 24) & 0xFF);
							str.push_back((nval >> 16) & 0xFF);
							str.push_back((nval >> 8) & 0xFF);
							str.push_back(nval & 0xFF);							
						break;
					}
	
					return true;
				}

				bool deserialize(const string& str, size_t& strIndex, size_t optData) {

					uint16_t len = str[optData] & 0x0F;

					OptValue::readOptDeltaOrLen(str, strIndex, len);

					uint32_t nval;
					switch (len) {

						case 0:
						break;

						case 1:
							m_ival = str[strIndex] & 0xFF;
							strIndex++;
						break;

						case 2:
							nval = (str[strIndex] << 8) & 0xFF00;
							nval |= (str[strIndex + 1] & 0xFF);
							nval &= 0xFFFF;
							strIndex += 2;
							m_ival = ntohs(nval);
						break;

						case 3:
							m_ival = (str[strIndex] << 16) & 0xFF0000;
							m_ival |= ((str[strIndex + 1] << 8) & 0xFF00);
							m_ival |= (str[strIndex + 2] & 0xFF);
							strIndex += 3;
						break;

						case 4:
							nval = (str[strIndex] << 24) & 0xFF000000;
							nval |= (str[strIndex + 1] << 16) & 0xFF0000;
							nval |= ((str[strIndex + 2] << 8) & 0xFF00);
							nval |= (str[strIndex + 3] & 0xFF);
							strIndex += 4;
							m_ival = ntohl(nval);
						break;

						default:
							LIB_MSG_ERR("OptVal<size_t> deserialize Un-support maxSiz: {}", len);
							return false;
						break;
					}

					return true;

				}

			private:
				uint32_t m_ival;
		};

		export template<>
			class OptVal<string> : public OptValue {
			public:

				OptVal(uint16_t minSiz, uint16_t maxSiz) : OptValue(minSiz, maxSiz) {
				}

				inline string get() {
					return m_sval;
				}

				inline void set(string sval) {
					m_sval = sval;
				}

				virtual bool serialize(string &str, size_t optData) {

					uint16_t len = (uint16_t) m_sval.length();

					OptValue::writeOptDeltaOrLen(str, len);
					str[optData] |= (len & 0x0F);

					str.append(m_sval);

					return true;
				}

				virtual bool deserialize(const string &str, size_t &strIndex, size_t optData) {

					uint16_t len = str[optData] & 0x0F;

					OptValue::readOptDeltaOrLen(str, strIndex, len);

					m_sval.append(str.substr(strIndex, len));
					strIndex += len;

					return true;

				}

			private:
				string m_sval;
		};

		export class Option : public OptionSerialize {
		public:

			enum class NUMBER { UNDEFINED = 0,
				IF_MATCH = 1, URI_HOST = 3, ETAG = 4, IF_NONE_MATCH = 5, OBSERVE = 6, URI_PORT = 7, LOCATION_PATH = 8,
				URI_PATH = 11, CONTENT_FORMAT = 12, MAX_AGE = 14, URI_QUERY = 15, ACCEPT = 17, LOCATION_QUERY = 20,
				BLOCK2 = 23, BLOCK1 = 27, SIZE2 = 28, PROXY_URI = 35, PROXY_SCHEME = 39, SIZE1 = 60, REQUEST_TAG = 292
			};

			static char DELIM_PATH;
			static char DELIM_QUERY;
			static char DELIM_IF_MATCH;
			static char DELIM_ETAG;

			Option() {}
			virtual ~Option() {}

			inline NUMBER getNumber() const {
				return m_number;
			}

			inline void setNumber(NUMBER numb) {
				m_number = numb;
			}

			inline void setVal(unique_ptr<OptValue> val) {
				m_val = std::move(val);
			}

			inline OptValue* getVal() const {
				return m_val.get();
			}

			static unique_ptr<OptValue> newVal(NUMBER numb) {

				switch (numb) {
					case NUMBER::IF_MATCH: {
						unique_ptr<OptVal<string>> val = make_unique<OptVal<string>>(0, 8);
						return val;
					}
					break;

					case NUMBER::URI_HOST:
						return make_unique<OptVal<string>>(1, 255);
					break;

					case NUMBER::ETAG: {
						unique_ptr<OptVal<string>> val = make_unique<OptVal<string>>(1, 8);
						return val;
					}
					break;

					case NUMBER::IF_NONE_MATCH:
						return make_unique<OptVal<string>>(0, 0);
					break;

					case NUMBER::OBSERVE:
						return make_unique<OptVal<size_t>>(0, 3);
					break;

					case NUMBER::BLOCK1:
					case NUMBER::BLOCK2:
						return make_unique<OptVal<Block>>(0, 3);
					break;

					case NUMBER::URI_PORT:
					case NUMBER::CONTENT_FORMAT:
					case NUMBER::ACCEPT:
						return make_unique<OptVal<size_t>>(0, 2);
					break;

					case NUMBER::LOCATION_PATH:
					case NUMBER::URI_PATH: {
						unique_ptr<OptVal<string>> val = make_unique<OptVal<string>>(0, 255);
						return val;
					}
					break;

					case NUMBER::URI_QUERY:
					case NUMBER::LOCATION_QUERY: {
						unique_ptr<OptVal<string>> val = make_unique<OptVal<string>>(0, 255);
						return val;
					}
					break;

					case NUMBER::MAX_AGE:
					case NUMBER::SIZE1:
					case NUMBER::SIZE2:
						return make_unique<OptVal<size_t>>(0, 4);
					break;

					case NUMBER::PROXY_URI:
						return make_unique<OptVal<string>>(1, 1034);
					break;

					case NUMBER::PROXY_SCHEME:
						return make_unique<OptVal<string>>(1, 255);
					break;

					case NUMBER::REQUEST_TAG:
						return make_unique<OptVal<string>>(0, 8);
					break;

					default:
						LIB_MSG_ERR("Option unknown creation of option number {}", static_cast<uint16_t>(numb));
						return make_unique<OptVal<string>>(0, 0);
					break;
				}
			}

			Block getBlockVal() {

				OptVal<Block>* optBlockVal = dynamic_cast<OptVal<Block>*>(m_val.get());
				if (!optBlockVal) {
					LIB_MSG_ERR_THROW_EX("Option {} is not block type", static_cast<uint32_t>(m_number));
				}

				return optBlockVal->get();
			}

			string getStrVal() {

				OptVal<string>* optStrVal = dynamic_cast<OptVal<string>*>(m_val.get());
				if (!optStrVal) {
					LIB_MSG_ERR_THROW_EX("Option {} is not string type", static_cast<uint32_t>(m_number));
				}

				return optStrVal->get();
			}

			size_t getNumVal() {

				OptVal<size_t>* optNumVal = dynamic_cast<OptVal<size_t>*>(m_val.get());
				if (!optNumVal) {
					LIB_MSG_ERR_THROW_EX("Option {} is not number type", static_cast<uint32_t>(m_number));
				}

				return optNumVal->get();
			}

			bool serialize(string& str, size_t prevOptNumb) {
				
				uint16_t delta = (uint16_t) (static_cast<size_t>(m_number) - prevOptNumb);

				size_t optData = str.size();

				// Create option delta and option length
				str.resize(str.size() + 1);

				OptValue::writeOptDeltaOrLen(str, delta);
				str[optData] = (delta << 4) & 0xF0;

				if (m_val == nullptr) {
					LIB_MSG_ERR_THROW_EX("Option has not assign a value");
				}

				return m_val->serialize(str, optData);
			}

			bool deserialize(const string& str, size_t& strIndex, size_t prevOptNumb) {

				size_t optData = strIndex;

				uint16_t optDelta = (str[strIndex] >> 4) & 0x0F;
				strIndex++;

				OptValue::readOptDeltaOrLen(str, strIndex, optDelta);
				m_number = static_cast<NUMBER>(optDelta + prevOptNumb);

				m_val = newVal(m_number);
				return m_val->deserialize(str, strIndex, optData);
			}

		private:

			NUMBER m_number{ NUMBER::UNDEFINED };
			unique_ptr<OptValue> m_val;
		};


		char Option::DELIM_PATH = '/';
		char Option::DELIM_QUERY = '&';
		char Option::DELIM_IF_MATCH = ';';
		char Option::DELIM_ETAG = ';';

	}
}
