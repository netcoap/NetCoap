//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

#include "Toolbox/Toolbox.h"

export module Toolbox:JsonValue;

import :LogMessage;

using namespace std;

namespace netcoap {
	namespace toolbox {

		export class JsonValue;
		export using MapJsonValType = unordered_map<string, unique_ptr<JsonValue>>;
		export using ListJsonValType = vector<unique_ptr<JsonValue>>;

		typedef union JsonAnyValue {

			JsonAnyValue() {}
			~JsonAnyValue() {}

			string strVal;
			int intVal;
			float floatVal;
			bool boolVal;
			char nullVal;
			ListJsonValType listVal;
			MapJsonValType mapVal;
		} JsonAnyValue;

		export class JsonValue {
		public:

			enum class VALUE_TYPE { UNDEFINED, STRING_VAL, INT_VAL, FLOAT_VAL, BOOL_VAL, NULL_VAL, LIST_VAL, MAP_VAL };

			VALUE_TYPE valType;
			JsonAnyValue val;

			JsonValue() { valType = VALUE_TYPE::UNDEFINED; };

			~JsonValue() {

				switch (valType) {
				case VALUE_TYPE::STRING_VAL:
					val.strVal.~string();
					break;

				case VALUE_TYPE::MAP_VAL:
					val.mapVal.~MapJsonValType();
					break;

				case VALUE_TYPE::LIST_VAL:
					val.listVal.~ListJsonValType();
					break;

				default:
					break;
				}
			}

			size_t getLength() {

				if (valType == VALUE_TYPE::LIST_VAL) {
					return val.listVal.size();
				}
				else if (valType == VALUE_TYPE::MAP_VAL) {
					return val.mapVal.size();
				}
				else
				{
					LIB_MSG_ERR_THROW_EX("Invalid Length");
					return 0;
				}
			}

			template <typename T>
			void newVal();
		};

		template<>
		void JsonValue::newVal<string>() {
			valType = VALUE_TYPE::STRING_VAL;
			new (&val.strVal) string();
		}

		template <>
		void JsonValue::newVal<MapJsonValType>() {
			valType = VALUE_TYPE::MAP_VAL;
			new (&val.mapVal) MapJsonValType();
		}

		template <>
		void JsonValue::newVal<ListJsonValType>() {
			valType = VALUE_TYPE::LIST_VAL;
			new (&val.listVal) ListJsonValType();
		}

		template <>
		void JsonValue::newVal<int>() {
			valType = VALUE_TYPE::INT_VAL;
			val.intVal = 0;
		}

		template <>
		void JsonValue::newVal<float>() {
			valType = VALUE_TYPE::FLOAT_VAL;
			val.floatVal = 0;
		}

		template <>
		void JsonValue::newVal<bool>() {
			valType = VALUE_TYPE::BOOL_VAL;
			val.boolVal = false;
		}

		template <>
		void JsonValue::newVal<char>() {
			valType = VALUE_TYPE::NULL_VAL;
			val.nullVal = 0;
		}
	}
}
