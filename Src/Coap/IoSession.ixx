//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

#include <openssl/ssl.h>

export module Coap:IoSession;

import Toolbox;

using namespace std;

using namespace netcoap::toolbox;

namespace netcoap {
	namespace coap {

		export class IoBuf {
		public:

			IpAddress clientAddr;
			string buf;
		};

		export class InpOutpResponse {
		public:
			virtual void* keepCompilerHappy() { return nullptr; }
		};

		export class ServerIoResponse : public InpOutpResponse {
		public:

			enum class STATUS_RETURN { NONE, NEW_CONNECTION, CLIENT_HAS_DATA };

			ServerIoResponse() {
				reset();
			}

			void reset() {
				memset(&clientAddr, 0, sizeof(clientAddr));
				statusRet = STATUS_RETURN::NONE;
				inpQ = nullptr;
			}

			IpAddress clientAddr;
			STATUS_RETURN statusRet = STATUS_RETURN::NONE;
			SyncQ<shared_ptr<IoBuf>>* inpQ = nullptr;
		};

		export class IoSession {
		public:

			IoSession() {}
			virtual ~IoSession() = default;

			virtual void init(JsonPropTree &cfg) = 0;
			
			virtual InpOutpResponse* getData() {
				return nullptr;
			};
			
			virtual void outDataToNet() {}

			virtual bool connect(void* ctx) {
				return false;
			};

			virtual bool disconnect(IpAddress ipAddr) {
				return true;
			}

			virtual int read(shared_ptr<IoBuf>& buf, void* ctx) {
				return -1;
			}

			virtual int write(shared_ptr<IoBuf> buf, void* ctx) {
				return -1;
			}

		protected:

			inline void setInitOnce(bool done) const {
				m_initOnce = done;
			}

			inline bool isInitOnce() {
				return m_initOnce;
			}

		private:
			static bool m_initOnce;
		};

		bool IoSession::m_initOnce = false;
	}
}