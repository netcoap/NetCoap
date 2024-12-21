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

#include <algorithm>
#include <fstream>
#include <ranges>
#include <stdexcept>
#include <format>
#include <iterator>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ranges>
#include <memory>
#include <cstring>

#include "Coap/Coap.h"
#include "Toolbox/Toolbox.h"

export module Toolbox:JsonPropTree;

import :JsonValue;
import :Helper;
import :LogMessage;

using namespace std;

namespace netcoap {
	namespace toolbox {

		class StrReader {
		public:

			StrReader(const string& s) : m_s(s) {
			}

			inline void get(char& ch) {
				ch = m_s[m_idx];
				m_idx++;
			}

			inline bool eof() const {
				return m_idx >= m_s.length();
			}

			inline void unget() {
				m_idx--;
			}

			inline void get(string& str, size_t num) {
				str.append(m_s.substr(m_idx, num));
				m_idx += num;
			}

		private:
			const string& m_s;
			size_t m_idx = 0;
		};

		export class JsonPropTree {
		public:

			JsonPropTree() {}

			~JsonPropTree() {}

			void fromJsonFile(const string jsonPathname) {

				ifstream file(jsonPathname, ios::binary | ios::ate);
				if (!file) {
					LIB_MSG_ERR_THROW_EX("Failed to open file {}", jsonPathname);
				}

				streamsize fileSize = file.tellg();
				file.seekg(0, std::ios::beg);

				string json;
				json.resize(fileSize);
				if (!file.read(json.data(), fileSize)) {
					LIB_MSG_ERR_THROW_EX("Failed to read file {}", jsonPathname);
				}

				file.close();

				StrReader strReader(json);

				init();

				beginParse(strReader);
			}

			void fromJsonStr(const string& jsonStr) {
				StrReader strReader(jsonStr);
				init();

				beginParse(strReader);
			}

			void toCborStr(string& cbor) {
				writeCbor(m_obj.get(), cbor);
			}

			void fromCborStr(const string& cbor) {
				uint8_t majorType = (cbor[0] >> CBOR_MAJOR_TYPE_SHIFT_BIT) & 0x7;
				if (majorType != CBOR_MAJOR_TYPE_MAP) {
					LIB_MSG_ERR_THROW_EX("Begin major type must be a map type, not {}", majorType);
				}

				StrReader strReader(cbor);

				init();
				m_obj = readCbor(strReader);
			}

			void print(ostream& out=cout) {
				size_t numSpaces = 0;
				print(m_obj.get(), out, numSpaces, true);
				out << "\n";
			}

			string toString() {
				ostringstream oss;
				size_t numSpaces = 0;
				print(m_obj.get(), oss, numSpaces, true);
				string json = oss.str();
				json.erase(std::remove_if(json.begin(), json.end(), [](unsigned char c) {
					return isspace(c);
					}), json.end());

				return json;
			}

			template <typename T>
			typename std::enable_if<!std::is_pointer<T>::value, T>::type
			get(const string path);

		    template <typename T>
			typename std::enable_if<std::is_pointer<T>::value, T>::type
    		get(const string path);

		private:

			static const uint8_t CBOR_MAJOR_TYPE_SHIFT_BIT = 5;
			static const uint8_t CBOR_MAJOR_TYPE_UNSIGNED_INT = 0;
			static const uint8_t CBOR_MAJOR_TYPE_NEGATIVE_INT = 1;
			static const uint8_t CBOR_MAJOR_TYPE_TEXT_STR = 3;
			static const uint8_t CBOR_MAJOR_TYPE_ARRAY = 4;
			static const uint8_t CBOR_MAJOR_TYPE_MAP = 5;
			static const uint8_t CBOR_MAJOR_TYPE_SIMPLE = 7;

			unique_ptr<JsonValue> m_obj;
			unsigned int m_lineNo;
			unsigned int m_totalBegEnd;
			unsigned int m_totalOpenCloseBracket;


			void init() {
				m_obj = nullptr;
				m_lineNo = 1;
				m_totalBegEnd = 0;
				m_totalOpenCloseBracket = 0;
			}

			inline void skipWhiteSpace(StrReader& strReader) {

				while (!strReader.eof()) {
					char ch; strReader.get(ch);
					if (ch == '\n') {
						m_lineNo++;
					}
					else if ((ch != ' ') && (ch != '\r') && (ch != '\t')) {
						strReader.unget();
						break;
					}
				}
			}

			void beginParse(StrReader& strReader) {

				enum class STATE { Init, JsonDone };

				skipWhiteSpace(strReader);
				if (strReader.eof())
					return;

				char ch; strReader.get(ch);

				switch (ch) {
				case '{':
					m_totalBegEnd++;
					m_obj = parseObject(strReader);
					break;

				default:
					LIB_MSG_ERR_THROW_EX("Line {}. Illegal object character {} is found", m_lineNo, ch);
					break;
				}

				skipWhiteSpace(strReader);

				if (m_totalOpenCloseBracket != 0) {
					LIB_MSG_ERR_THROW_EX("Line {}. Invalid JSON strReader. Found [ and ] not match", m_lineNo);
				}

				if (m_totalBegEnd != 0) {
					LIB_MSG_ERR_THROW_EX("Line {}. Invalid JSON strReader. Found {{ and }} not match", m_lineNo);
				}

				if (!strReader.eof()) {
					LIB_MSG_ERR_THROW_EX("Line {}. Invalid JSON strReader. Found unknown characters following end of JSON object", m_lineNo);
				}
			}

			unique_ptr<JsonValue> parseObject(StrReader& strReader) {

				enum class STATE { Init, LvalFound, RvalFound, NextKeyVal };

				unique_ptr<JsonValue> obj = nullptr;
				unique_ptr<JsonValue> lval = nullptr;
				unique_ptr<JsonValue> rval = nullptr;

				bool done = false;
				STATE curState, nxtState = STATE::Init;

				while (!done && !strReader.eof()) {
					skipWhiteSpace(strReader);
					if (strReader.eof())
						break;

					curState = nxtState;

					char ch; strReader.get(ch);

					switch (ch) {
					case '"':
						if (curState == STATE::Init) {
							lval = parseString(strReader);

							obj = make_unique<JsonValue>();
							obj->newVal<MapJsonValType>();

							nxtState = STATE::LvalFound;
						}
						else if (curState == STATE::NextKeyVal) {
							lval = parseString(strReader);

							nxtState = STATE::LvalFound;
						}
						else {
							LIB_MSG_ERR_THROW_EX("Line {}. Syntax error while parsing object with {}", m_lineNo, ch);
						}
						break;

					case ':':
						if (curState == STATE::LvalFound) {
							rval = parseValue(strReader);
							if (obj->val.mapVal.find(lval->val.strVal) != obj->val.mapVal.end()) {
								LIB_MSG_ERR_THROW_EX("Line {}.Found duplicate key {} in object", m_lineNo, lval->val.strVal);
							}
							else {
								obj->val.mapVal.emplace(lval->val.strVal, std::move(rval));
							}

							nxtState = STATE::RvalFound;
						}
						else {
							LIB_MSG_ERR_THROW_EX("Line {}. Syntax error while parsing object with {}", m_lineNo, ch);
						}
						break;

					case ',':
						if (curState == STATE::RvalFound) {

							nxtState = STATE::NextKeyVal;
						}
						else {
							LIB_MSG_ERR_THROW_EX("Line {}. Syntax error while parsing object with {}", m_lineNo, ch);
						}
						break;

					case '}':
						if ((curState == STATE::Init) || (curState == STATE::RvalFound)) {
							m_totalBegEnd--;
							done = true;
						}
						else {
							LIB_MSG_ERR_THROW_EX("Line {}. Syntax error while parsing object with {}", m_lineNo, ch);
						}
						break;

					default:
						LIB_MSG_ERR_THROW_EX("Line {}. Syntax error while parsing object with {}", m_lineNo, ch);
						break;
					}
				}

				return obj;
			}

			unique_ptr<JsonValue> parseArray(StrReader& strReader) {

				unique_ptr<JsonValue> list = make_unique<JsonValue>();
				list->newVal<ListJsonValType>();

				skipWhiteSpace(strReader);
				if (strReader.eof()) {
					return list;
				}

				char ch; strReader.get(ch);
				if (ch == ']') {
					return list;
				}

				strReader.unget();

				list->val.listVal.push_back(parseValue(strReader));

				bool done = false;

				while (!done && !strReader.eof()) {
					skipWhiteSpace(strReader);
					if (strReader.eof())
						break;

					char ch; strReader.get(ch);

					switch (ch) {
					case ']':
						m_totalOpenCloseBracket--;
						done = true;
						break;

					case ',':
						list->val.listVal.push_back(parseValue(strReader));
						break;

					default:
						LIB_MSG_ERR_THROW_EX("Line {}. Syntax error while parsing array with {}", m_lineNo, ch);
						break;
					}
				}

				return list;
			}

			unique_ptr<JsonValue> parseValue(StrReader& strReader) {

				skipWhiteSpace(strReader);
				if (strReader.eof()) {
					LIB_MSG_ERR_THROW_EX("Line {}. Expected value not found", m_lineNo);
					return nullptr;
				}

				char ch; strReader.get(ch);

				switch (ch) {
				case '"':
					return parseString(strReader);
					break;

				case '{':
					m_totalBegEnd++;
					return parseObject(strReader);
					break;

				case '[':
					m_totalOpenCloseBracket++;
					return parseArray(strReader);
					break;

				case '-':
				case '+':
					strReader.unget();
					return parseNumber(strReader);
					break;

				case 't':
				case 'f':
					strReader.unget();
					return parseBoolean(strReader);
					break;

				case 'n':
					strReader.unget();
					return parseNull(strReader);
					break;

				default:
					if ((ch >= '0') && (ch <= '9')) {
						strReader.unget();
						return parseNumber(strReader);
					}

					LIB_MSG_ERR_THROW_EX("Line {}. Syntax error while parsing value with {}", m_lineNo, ch);
					return nullptr;

					break;
				}
			}

			unique_ptr<JsonValue> parseString(StrReader& strReader) {

				unique_ptr<JsonValue> val = make_unique<JsonValue>();
				val->newVal<string>();

				bool done = false;
				char lastCh = '\0';
				char ch;
				while (!done && !strReader.eof()) {
					strReader.get(ch);
					if ((ch == '"') && (lastCh != '\\')) {
						done = true;
					}
					else {
						val->val.strVal.push_back(ch);
					}

					lastCh = ch;
				}

				if (!done) {
					LIB_MSG_ERR_THROW_EX("Line {}. Unable to find enclosing string \"", m_lineNo);
				}

				return val;
			}

			unique_ptr<JsonValue> parseNumber(StrReader& strReader) {

				unique_ptr<JsonValue> val = make_unique<JsonValue>();

				bool done = false;
				string sVal;
				bool isFloat = false;
				while (!done && !strReader.eof()) {
					char ch; strReader.get(ch);
					if ((ch == '.') || (ch == 'e') || (ch == 'E')) {
						isFloat = true;
					}
					else if (!isdigit(ch) && (ch != '-') && (ch != '+')) {
						done = true;
						strReader.unget();
						continue;
					}

					sVal.push_back(ch);
				}

				if (sVal.length() == 0) {
					LIB_MSG_ERR_THROW_EX("Line {}. Unable to find number value", m_lineNo);
				}

				try {
					if (isFloat) {
						val->newVal<float>();
						val->val.floatVal = stof(sVal);
					}
					else {
						val->newVal<int>();
						val->val.intVal = stoi(sVal);
					}
				}
				catch (...) {
					LIB_MSG_ERR_THROW_EX("Line {}. Malform number {}", m_lineNo, sVal);
				}

				return val;
			}

			unique_ptr<JsonValue> parseBoolean(StrReader& strReader) {

				unique_ptr<JsonValue> val = make_unique<JsonValue>();

				string str;
				char ch;
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}

				if (str.length() != 4) {
					LIB_MSG_ERR_THROW_EX("Line {}. Expected true or false but found {}", m_lineNo, str);
				}

				if (str == "true") {
					val->newVal<bool>();
					val->val.boolVal = true;
				}
				else {
					if (!strReader.eof()) {
						char ch; strReader.get(ch); str.push_back(ch);
						if (str == "false") {
							val->newVal<bool>();
							val->val.boolVal = false;
						}
						else {
							LIB_MSG_ERR_THROW_EX("Line {}. Expected true or false but found {}", m_lineNo, str);
						}
					}
					else {
						LIB_MSG_ERR_THROW_EX("Line {}. Expected true or false but found {}", m_lineNo, str);
					}
				}

				return val;
			}

			unique_ptr<JsonValue> parseNull(StrReader& strReader) {

				unique_ptr<JsonValue> val = make_unique<JsonValue>();

				string str;
				char ch;
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}
				if (!strReader.eof()) {
					strReader.get(ch); str.push_back(ch);
				}

				if (str.length() != 4) {
					LIB_MSG_ERR_THROW_EX("Line {}. Expected null but found {}", m_lineNo, str);
				}

				if (str == "null") {
					val->newVal<char>();
					val->val.nullVal = 0;
				}
				else {
					LIB_MSG_ERR_THROW_EX("Line {}. Expected null but found {}", m_lineNo, str);
				}

				return val;
			}

			void writeCbor(const JsonValue* val, string& cborBuf) {
				
				switch (val->valType) {

					case JsonValue::VALUE_TYPE::BOOL_VAL:
						writeCborBool(val->val.boolVal, cborBuf);
					break;

					case JsonValue::VALUE_TYPE::FLOAT_VAL:
						writeCborFloat(val->val.floatVal, cborBuf);
					break;

					case JsonValue::VALUE_TYPE::INT_VAL:
						writeCborInt(val->val.intVal, cborBuf);
					break;

					case JsonValue::VALUE_TYPE::LIST_VAL:
						writeCborArray(val->val.listVal, cborBuf);
					break;

					case JsonValue::VALUE_TYPE::MAP_VAL:
						writeCborMap(val->val.mapVal, cborBuf);
					break;

					case JsonValue::VALUE_TYPE::NULL_VAL:
						writeCborNull(val->val.nullVal, cborBuf);
					break;

					case JsonValue::VALUE_TYPE::STRING_VAL:
						writeCborTxt(val->val.strVal, cborBuf);
					break;

					default:
						LIB_MSG_ERR_THROW_EX("Undefined Val Type {} not supported", static_cast<uint8_t>(val->valType));
					break;
				}
			}

			void writeCborBool(bool boolVal, string& cborBuf) {

				uint8_t val = (CBOR_MAJOR_TYPE_SIMPLE << CBOR_MAJOR_TYPE_SHIFT_BIT) |
					(boolVal + 20);
				cborBuf.push_back(val);
			}

			void writeCborFloat(float floatVal, string& cborBuf) {

				uint32_t netIntFloat;
				memcpy(&netIntFloat, &floatVal, sizeof(float));
				netIntFloat = htonl(netIntFloat);

				uint8_t majorAddInfo = (CBOR_MAJOR_TYPE_SIMPLE << CBOR_MAJOR_TYPE_SHIFT_BIT) |
					26;
				cborBuf.push_back(majorAddInfo);

				size_t idx = cborBuf.length();
				cborBuf.resize(cborBuf.length() + 4);
				memcpy((void*) (cborBuf.c_str() + idx), &netIntFloat, 4);				
			}

			void writeCborInt(uint8_t majorType, int intVal, string& cborBuf) {

				if (intVal < 0) {
					intVal = -1 - intVal;
				}

				uint8_t majorAddInfo;
				if (intVal & 0xFFFF0000) { // Int with following 4 bytes
					majorAddInfo = (majorType << CBOR_MAJOR_TYPE_SHIFT_BIT) |
						26;
					cborBuf.push_back(majorAddInfo);

					intVal = htonl(intVal);
					cborBuf.push_back(intVal & 0xFF);
					cborBuf.push_back((intVal >> 8) & 0xFF);
					cborBuf.push_back((intVal >> 16) & 0xFF);
					cborBuf.push_back((intVal >> 24) & 0xFF);
				}
				else if (intVal & 0xFF00) { // Int with following 2 bytes
					majorAddInfo = (majorType << CBOR_MAJOR_TYPE_SHIFT_BIT) |
						25;
					cborBuf.push_back(majorAddInfo);

					intVal = htons(intVal);
					cborBuf.push_back(intVal & 0xFF);
					cborBuf.push_back((intVal >> 8) & 0xFF);
				}
				else if (intVal > 23) { // Int with following 1 byte
					majorAddInfo = (majorType << CBOR_MAJOR_TYPE_SHIFT_BIT) |
						24;
					cborBuf.push_back(majorAddInfo);

					cborBuf.push_back(intVal & 0xFF);
				}
				else {
					majorAddInfo = (majorType << CBOR_MAJOR_TYPE_SHIFT_BIT) |
						intVal;
					cborBuf.push_back(majorAddInfo);
				}

			}

			void writeCborInt(int intVal, string& cborBuf) {

				uint8_t majorType = CBOR_MAJOR_TYPE_UNSIGNED_INT;
				if (intVal < 0) {
					majorType = CBOR_MAJOR_TYPE_NEGATIVE_INT;
				}

				writeCborInt(majorType, intVal, cborBuf);
			}

			void writeCborArray(const ListJsonValType& listVal, string& cborBuf) {

				writeCborInt(CBOR_MAJOR_TYPE_ARRAY, (int) listVal.size(), cborBuf);

				for (auto& ptr : listVal) {
					writeCbor(ptr.get(), cborBuf);
				}
			}

			void writeCborMap(const MapJsonValType& mapVal, string& cborBuf) {

				writeCborInt(CBOR_MAJOR_TYPE_MAP, (int) mapVal.size(), cborBuf);

				for (auto& [key, value] : mapVal) {
					writeCborTxt(key, cborBuf);
					writeCbor(value.get(), cborBuf);
				}
			}

			void writeCborNull(char null, string& cborBuf) {

				uint8_t val = (CBOR_MAJOR_TYPE_SIMPLE << CBOR_MAJOR_TYPE_SHIFT_BIT) |
					null + 22;
				cborBuf.push_back(val);
			}

			void writeCborTxt(const string& strVal, string& cborBuf) {
				writeCborInt(CBOR_MAJOR_TYPE_TEXT_STR, (int) strVal.length(), cborBuf);
				cborBuf.append(strVal);
			}

			unique_ptr<JsonValue> readCbor(StrReader& strReader) {

				char ch; strReader.get(ch); strReader.unget();
				uint8_t majorType = (ch >> CBOR_MAJOR_TYPE_SHIFT_BIT) & 0x7;
				switch (majorType) {

					case CBOR_MAJOR_TYPE_UNSIGNED_INT:
						return readCborInt(strReader);
					break;

					case CBOR_MAJOR_TYPE_NEGATIVE_INT:
						return readCborInt(strReader);
					break;

					case CBOR_MAJOR_TYPE_TEXT_STR:
						return readCborTxt(strReader);
					break;

					case CBOR_MAJOR_TYPE_ARRAY:
						return readCborArray(strReader);
					break;

					case CBOR_MAJOR_TYPE_MAP:
						return readCborMap(strReader);
					break;

					case CBOR_MAJOR_TYPE_SIMPLE: // Float, bool, or null
						return readCborSimple(strReader);
					break;

					default:
						LIB_MSG_ERR_THROW_EX("Undefined Major Type {} not supported", majorType);
						return nullptr;
					break;
				}
			}

			int readCborUint(uint8_t addInfo, StrReader& strReader) {

				char ch; int intVal;
				switch (addInfo) {
					case 24: // 1 byte
						strReader.get(ch);
						intVal = ch & 0xFF;
					break;

					case 25: // 2 bytes
						strReader.get(ch);
						intVal = ch & 0xFF;
						strReader.get(ch);
						intVal |= ((ch << 8) & 0xFF00);
						intVal = ntohs(intVal);
					break;

					case 26: // 4 bytes
						strReader.get(ch);
						intVal = ch & 0xFF;
						strReader.get(ch);
						intVal |= ((ch << 8) & 0xFF00);
						strReader.get(ch);
						intVal |= ((ch << 16) & 0xFF0000);
						strReader.get(ch);
						intVal |= ((ch << 24) & 0xFF000000);
						intVal = ntohl(intVal);
					break;

					case 27:
						LIB_MSG_ERR_THROW_EX("64 bits integer not supported");
						return 0;
					break;

					default:
						intVal = addInfo;
					break;
				}

				return intVal;
			}

			unique_ptr<JsonValue> readCborInt(StrReader& strReader) {

				unique_ptr<JsonValue> val = make_unique<JsonValue>();

				char majorAddInfo; strReader.get(majorAddInfo);
				
				uint8_t addInfo = majorAddInfo & 0x1F;
				int intVal = readCborUint(addInfo, strReader);

				uint8_t majorType = majorAddInfo >> CBOR_MAJOR_TYPE_SHIFT_BIT;
				if (majorType == CBOR_MAJOR_TYPE_NEGATIVE_INT) {
					intVal = -intVal - 1;
				}

				val->newVal<int>();
				val->val.intVal = intVal;

				return val;
			}

			unique_ptr<JsonValue> readCborTxt(StrReader& strReader) {

				unique_ptr<JsonValue> val = make_unique<JsonValue>();

				char majorAddInfo; strReader.get(majorAddInfo);

				uint8_t addInfo = majorAddInfo & 0x1F;
				int len = readCborUint(addInfo, strReader);

				val->newVal<string>();
				strReader.get(val->val.strVal, len);

				return val;
			}

			unique_ptr<JsonValue> readCborArray(StrReader& strReader) {

				unique_ptr<JsonValue> list = make_unique<JsonValue>();
				list->newVal<ListJsonValType>();

				char majorAddInfo; strReader.get(majorAddInfo);

				uint8_t addInfo = majorAddInfo & 0x1F;
				int len = readCborUint(addInfo, strReader);
				for (int i = 0; i < len; i++) {
					list->val.listVal.push_back(readCbor(strReader));
				}

				return list;
			}

			unique_ptr<JsonValue> readCborMap(StrReader& strReader) {

				unique_ptr<JsonValue> map = make_unique<JsonValue>();
				map->newVal<MapJsonValType>();

				char majorAddInfo; strReader.get(majorAddInfo);

				uint8_t addInfo = majorAddInfo & 0x1F;
				int len = readCborUint(addInfo, strReader);
				for (int i = 0; i < len; i++) {
					unique_ptr<JsonValue> key = readCborTxt(strReader);
					if (map->val.mapVal.find(key->val.strVal) != map->val.mapVal.end()) {
						LIB_MSG_ERR_THROW_EX("Found duplicate key {} in map", key->val.strVal);
					}
					map->val.mapVal.emplace(key->val.strVal, readCbor(strReader));
				}

				return map;
			}

			unique_ptr<JsonValue> readCborSimple(StrReader& strReader) {

				unique_ptr<JsonValue> val = make_unique<JsonValue>();

				char majorAddInfo; strReader.get(majorAddInfo);

				char ch;
				uint8_t addInfo = majorAddInfo & 0x1F;
				switch (addInfo) {
				
					case 20: // false
					case 21: // true
						val->newVal<bool>();
						val->val.boolVal = (addInfo == 21);
					break;
						
					case 22: // null
						val->newVal<char>();
						val->val.nullVal = 0;
					break;

					case 26: { // 32 bit float
						val->newVal<float>();
						
						uint32_t netIntFloat;
						strReader.get(ch);
						netIntFloat = ch & 0xFF;
						strReader.get(ch);
						netIntFloat |= ((ch << 8) & 0xFF00);
						strReader.get(ch);
						netIntFloat |= ((ch << 16) & 0xFF0000);
						strReader.get(ch);
						netIntFloat |= ((ch << 24) & 0xFF000000);

						netIntFloat = ntohl(netIntFloat);
						memcpy(&val->val.floatVal, &netIntFloat, sizeof(float));
					}
					break;

					default:
						LIB_MSG_ERR_THROW_EX("Only support floating point");
						return nullptr;
					break;
				}

				return val;
			}

			void print(JsonValue* val, ostream& out, size_t& numSpaces, bool arrayOfSimpleType) {

				string spaces;
				switch (val->valType) {

					case JsonValue::VALUE_TYPE::BOOL_VAL:
						if (val->val.boolVal) {
							out << "true";
						}
						else {
							out << "false";
						}
					break;

					case JsonValue::VALUE_TYPE::FLOAT_VAL:
						out << fixed << setprecision(3);
						out << val->val.floatVal;
					break;

					case JsonValue::VALUE_TYPE::INT_VAL:
						out << val->val.intVal;
					break;

					case JsonValue::VALUE_TYPE::LIST_VAL:
						numSpaces += 2;
						if (arrayOfSimpleType) {
							spaces.assign(numSpaces, ' ');
							out << "\n";
							out << spaces << "[\n";
							numSpaces += 2;
						}
						else {
							out << "[\n";
						}
						for (int i = 0; i < val->val.listVal.size(); i++) {
							spaces.assign(numSpaces, ' ');
							out << spaces;
							if ((val->val.listVal[i]->valType == JsonValue::VALUE_TYPE::LIST_VAL) ||
								(val->val.listVal[i]->valType == JsonValue::VALUE_TYPE::MAP_VAL)) {
								arrayOfSimpleType = false;
							}
							else {
								arrayOfSimpleType = true;
							}
							print(val->val.listVal[i].get(), out, numSpaces, arrayOfSimpleType);
							if (i != (val->val.listVal.size() - 1)) {
								out << ",";
							}
							out << "\n";
						}

						if (arrayOfSimpleType) {
							numSpaces -= 2;
							spaces.assign(numSpaces, ' ');
							out << spaces << "]";
						}
						else {
							spaces.assign(numSpaces, ' ');
							out << spaces << "]";
						}
						numSpaces -= 2;
						break;

					case JsonValue::VALUE_TYPE::MAP_VAL: {
						numSpaces += 2;
						if (arrayOfSimpleType) {
							spaces.assign(numSpaces, ' ');
							out << "\n";
							out << spaces << "{\n";
							numSpaces += 2;
						}
						else {
							out << "{\n";
						}

						for (auto iter = val->val.mapVal.begin(); iter != val->val.mapVal.end(); ++iter) {
							spaces.assign(numSpaces, ' ');
							out << spaces << '"' + iter->first + '"' << ": ";
							print(iter->second.get(), out, numSpaces, true);
							if (next(iter) != val->val.mapVal.end()) {
								out << ",";
							}
							out << "\n";
						}

						if (arrayOfSimpleType) {
							numSpaces -= 2;
							spaces.assign(numSpaces, ' ');
							out << spaces << "}";
						}
						else {
							spaces.assign(numSpaces - 2, ' ');
							out << spaces << "}";
						}
						numSpaces -= 2;
					}
					break;

					case JsonValue::VALUE_TYPE::NULL_VAL:
						out << "null";
					break;

					case JsonValue::VALUE_TYPE::STRING_VAL:
						out << '"' + val->val.strVal + '"';
					break;

					default:
						LIB_MSG_ERR_THROW_EX("Undefined Val Type {} not supported", static_cast<uint8_t>(val->valType));
					break;
				}
			}
		};

		template<>
		JsonValue* JsonPropTree::get<JsonValue*>(const string path) {

			if (m_obj == nullptr) {
				LIB_MSG_ERR_THROW_EX("Line {}. Expected JSON format not found", m_lineNo);
				return nullptr;
			}

			stringstream ssPath(path);

			string key;
			JsonValue* jv = m_obj.get();
			while (getline(ssPath, key, '.')) {

				if (jv == nullptr) {
					LIB_MSG_ERR_THROW_EX("Expected not null in property {}", key);
				}
				else if ((key.length() >= 3) && (key[0] == '[') && (key[key.length() - 1] == ']')) {
					if (jv->valType != JsonValue::VALUE_TYPE::LIST_VAL) {
						LIB_MSG_ERR_THROW_EX("Expected array {} not found", key);
					}
					string arrIdxStr = key.substr(1, key.length() - 2);
					bool allDigits = ranges::all_of(arrIdxStr, [](char ch) {
						return isdigit(ch);
						});
					if (!allDigits) {
						LIB_MSG_ERR_THROW_EX("Expected array index number {}", arrIdxStr);
					}

					size_t arrIdx = stoi(arrIdxStr);
					if (arrIdx >= jv->val.listVal.size()) {
						LIB_MSG_ERR_THROW_EX("Expected not indexing {} exceeded list size", arrIdxStr);
					}

					jv = jv->val.listVal[arrIdx].get();
				}
				else if (jv->valType == JsonValue::VALUE_TYPE::MAP_VAL) {
					if (jv->val.mapVal.find(key) == jv->val.mapVal.end()) {
						LIB_MSG_ERR_THROW_EX("Expected property {} not found", key);
					}

					jv = jv->val.mapVal.at(key).get();
				}
				else {
					LIB_MSG_ERR_THROW_EX("Expected not {} an object name or array of value", key);
				}
			}

			return jv;
		}

		template <>
		string JsonPropTree::get<string>(const string path) {

			JsonValue* v = this->get<JsonValue*>(path);
			if ((v == nullptr) || (v->valType != JsonValue::VALUE_TYPE::STRING_VAL)) {
				LIB_MSG_ERR_THROW_EX("Value not a string");
				return "";
			}
			else {
				return v->val.strVal;
			}
		}

		template <>
		int JsonPropTree::get<int>(const string path) {

			JsonValue* v = this->get<JsonValue*>(path);
			if ((v == nullptr) || (v->valType != JsonValue::VALUE_TYPE::INT_VAL)) {
				LIB_MSG_ERR_THROW_EX("Value not an integer");
				return 0;
			}
			else {
				return v->val.intVal;
			}
		}

		template <>
		float JsonPropTree::get<float>(const string path) {

			JsonValue* v = this->get<JsonValue*>(path);
			if ((v == nullptr) || (v->valType != JsonValue::VALUE_TYPE::FLOAT_VAL)) {
				LIB_MSG_ERR_THROW_EX("Value not a type float");
				return 0;
			}
			else {
				return v->val.floatVal;
			}
		}

		template <>
		bool JsonPropTree::get<bool>(const string path) {

			JsonValue* v = this->get<JsonValue*>(path);
			if ((v == nullptr) || (v->valType != JsonValue::VALUE_TYPE::BOOL_VAL)) {
				LIB_MSG_ERR_THROW_EX("Value not type true or false");
				return false;
			}
			else {
				return v->val.boolVal;
			}
		}

		template <>
		char JsonPropTree::get<char>(const string path) {

			JsonValue* v = this->get<JsonValue*>(path);
			if ((v == nullptr) || (v->valType != JsonValue::VALUE_TYPE::NULL_VAL)) {
				LIB_MSG_ERR_THROW_EX("Value not type null");
				return 0;
			}
			else {
				return v->val.nullVal;
			}
		}
	}
}