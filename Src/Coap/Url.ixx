//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <algorithm>
#include <string>
#include <unordered_map>
#include <ranges>
#include <format>
#include <cstdint>

#include "Toolbox/Toolbox.h"

export module Coap:Url;

import Toolbox;

using namespace std;

using namespace netcoap::toolbox;

namespace netcoap {
	namespace coap {
		
		// Implement absolute Uri as per RFC 7252 Section 6.4

		export class Url {
		public:

			Url() {}
			~Url() {}

			Url(const string url) {
				setUrl(url);
			}

			void setUrl(const string url) {

				if (url.find('#') != string::npos) {
					LIB_MSG_ERR_THROW_EX("Fragment URL not supported for {}", url);
				}

				size_t idx = 0;
				m_queries.clear();

				m_scheme = parseScheme(url, idx);
				if ((url.length() <= (idx + 1)) || (url.at(idx) != '/') || (url.at(idx + 1) != '/')) {
					LIB_MSG_ERR_THROW_EX("Missing // in url {}", url);
				}
				idx += 2;
				m_host = parseHost(url, idx);
				m_port = parsePort(url, idx);
				m_path = parsePath(url, idx);

				if (idx < url.length()) {
					size_t qmarkIdx = url.find('?');
					if (qmarkIdx != string::npos) {
						parseQuery(url, qmarkIdx + 1);
					}
				}
			}

			void setScheme(const string name) {
				size_t idx = 0;
				string scheme = parseScheme(name, idx);
				if (idx != name.length()) {
					LIB_MSG_ERR_THROW_EX("Invalid scheme {}", name);
				}
			}

			string getScheme() const {
				return m_scheme;
			}

			void setHost(const string host) {
				size_t idx = 0;
				string ipHost = parseHost(host, idx);
				if (idx != host.length()) {
					LIB_MSG_ERR_THROW_EX("Invalid host {}", host);
				}
			}

			inline string getHost() const {
				return m_host;
			}

			inline void setPort(uint16_t port) {
				m_port = port;
			}

			inline uint16_t getPort() const {
				return m_port;
			}

			void setPath(const string path) {
				size_t idx = 0;
				m_path = parsePath(path, idx);
				if (idx != path.length()) {
					LIB_MSG_ERR_THROW_EX("Invalid path {}", path);
				}
			}
			
			string getPath() const {
				string path = m_path;
				path = specialQryChToHex(path);
				ranges::replace(path, ' ', '+');
				return "/" + path;
			}

			void addQuery(const string query) {
				parseQuery(query, 0);
			}

			string getQuery(const string key) const {
				string k = key;
				ranges::replace(k, '+', ' ');
				k = percentHexToChar(k);
				
				if (m_queries.find(k) != m_queries.end()) {
					string v = m_queries.at(k);
					v = specialQryChToHex(v);
					ranges::replace(v, ' ', '+');
					return v;
				}

				return "";
			}

			string getQueries() const {
				string queries;
				bool addAnd = false;
				for (const auto& [key, value] : m_queries) {
					if (addAnd) {
						queries += "&";
					}
					
					string lval = key;
					lval = specialQryChToHex(lval);
					ranges::replace(lval, ' ', '+');

					string rval = value;
					rval = specialQryChToHex(rval);
					ranges::replace(rval, ' ', '+');
					queries += lval + "=" + rval;
					addAnd = true;
				}

				return queries;
			}

			string getUrl() const {
				string url = m_scheme + "://" + getHost() + ":" + to_string(getPort()) +
					getPath() + "?" + getQueries();
				return url;
			}

		private:

			string m_scheme, m_host, m_path;
			uint16_t m_port;
			unordered_map<string, string> m_queries;
			static const string RESERVED_QUERY_CHAR;

			string specialQryChToHex(const string& str) const {
				size_t idx = 0;
				string newStr;
				while (idx < str.length()) {
					size_t found = str.find_first_of(RESERVED_QUERY_CHAR, idx);
					if (found == string::npos) {
						newStr.append(str.substr(idx));
						break;
					}

					newStr.append(str.substr(idx, found - idx));
					char ch = str.at(found);
					string hexStr = format("%{:02X}", ch);
					newStr.append(hexStr);

					idx = found + 1;
				}

				return newStr;
			}

			string percentHexToChar(const string& str) const {
				size_t idx = 0;
				size_t percentIdx;
				string newStr;

				while (idx < str.length()) {
					percentIdx = str.find('%', idx);
					if (percentIdx == string::npos) {
						newStr.append(str.substr(idx));
						break;
					}
					else {
						newStr.append(str.substr(idx, percentIdx - idx));
						if ((percentIdx + 2) >= str.length()) {
							LIB_MSG_ERR_THROW_EX("Invalid % found in {}. % must follow by 2 digits hex", str);
						}

						unsigned char ch = 0;
						try {
							ch = stoi(str.substr(percentIdx + 1, 2), nullptr, 16);
						}
						catch (...) {
							LIB_MSG_ERR_THROW_EX("Found % illegal hex in {}", str);
						}

						newStr.push_back(ch);
						idx = percentIdx + 3;
					}
				}

				return newStr;
			}

			string parseScheme(const string &url, size_t &idx) {
				string scheme;
				if (idx >= url.length()) {
					return scheme;
				}

				idx = url.find(':');
				if (idx != string::npos) {
					scheme = url.substr(0, idx);
				} else {
					idx = url.length();
					scheme = url.substr(0, idx);
					idx--;
				}

				scheme = Helper::toLower(scheme);
				if ((scheme != COAP) && (scheme != COAPS)) {
					LIB_MSG_ERR_THROW_EX("Invalid scheme {}", scheme);
				}

				idx++;
				return scheme;
			}

			string parseHost(const string &host, size_t &idx) {
				string shost;
				if (idx >= host.length()) {
					return shost;
				}

				size_t endIdx = host.find(':', idx);
				if (endIdx != string::npos) {
					shost = host.substr(idx, endIdx - idx);
				} else {
					endIdx = host.find('/', idx);
					if (endIdx != string::npos) {
						shost = host.substr(idx, endIdx - idx);
					} else {
						endIdx = host.length();
						shost = host.substr(idx, endIdx - idx);
					}
					endIdx--;
				}

				shost = percentHexToChar(shost);
				shost = Helper::toLower(shost);
				IpAddress ipAddr(shost, 1);
				
				idx = endIdx + 1;

				return ipAddr.getAddress();
			}

			uint16_t parsePort(const string &port, size_t &idx) {
				string sport;
				uint16_t iport = 0;
				if (idx >= port.length()) {
					return iport;
				}

				size_t endIdx = port.find('/', idx);
				if (endIdx == string::npos) {
					endIdx = port.length();
				}


				if (endIdx == idx) {
					return iport;
				}

				sport = port.substr(idx, endIdx - idx);

				try {
					iport = (uint16_t)stoul(sport);
				} catch (...) {
					LIB_MSG_ERR_THROW_EX("Invalid port number found in {}", sport);
				}

				idx = endIdx;

				return iport;
			}

			string parsePath(const string &path, size_t &idx) {
				string spath;
				if (idx >= path.length()) {
					return spath;
				}
				if (path.at(idx) != '/') {
					LIB_MSG_ERR_THROW_EX("Path must begin with / in {}", path.substr(idx));
				}
				
				size_t endIdx = path.find('?', idx);
				if (endIdx == string::npos) {
					endIdx = path.length();
				}
				
				spath = path.substr(idx + 1, endIdx - idx - 1);
				ranges::replace(spath, '+', ' ');
				spath = percentHexToChar(spath);

				idx = endIdx;

				return spath;
			}

			void parseQuery(const string& path, size_t idx) {

				while (idx < path.length()) {

					size_t lvalIdx = path.find('=', idx);
					if (lvalIdx == string::npos) {
						LIB_MSG_ERR_THROW_EX("Lvalue not found in {}", path.substr(idx));
					}
					string lval = path.substr(idx, lvalIdx - idx);
					ranges::replace(lval, '+', ' ');
					lval = percentHexToChar(lval);

					if ((lvalIdx + 1) >= path.length()) {
						LIB_MSG_ERR_THROW_EX("Rvalue not found in {}", path.substr(idx));
					}
					size_t rvalIdx = path.find('&', idx);
					string rval;
					if (rvalIdx == string::npos) {
						rval = path.substr(lvalIdx + 1);
						rvalIdx = path.length();
					}
					else {
						rval = path.substr(lvalIdx + 1, rvalIdx - lvalIdx - 1);
					}
					ranges::replace(rval, '+', ' ');
					rval = percentHexToChar(rval);

					if (m_queries.find(lval) != m_queries.end()) {
						LIB_MSG_ERR_THROW_EX("Duplicate key {} found. Not support duplicate key", lval);
					}

					m_queries.emplace(lval, rval);

					size_t andOpIdx = path.find('&', rvalIdx);
					if (andOpIdx == string::npos) {
						break;
					}

					idx = andOpIdx + 1;
				}
			}
		};

		const string Url::RESERVED_QUERY_CHAR { '/', '?', '&', '=', '+' };
	}
}
