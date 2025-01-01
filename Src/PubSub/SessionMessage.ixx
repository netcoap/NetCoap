//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <iostream>
#include <string>

export module PubSub:SessionMessage;

import Coap;

using namespace std;

using namespace netcoap::coap;

namespace netcoap {
	namespace pubsub {

		export class SessionMessage : public NetCoapMessage {
		public:

			enum class REQUEST {SESSION_EXIT};

			inline void setRequest(REQUEST req) {
				m_req = req;
			}

			inline REQUEST getRequest() {
				return m_req;
			}

		private:

			REQUEST m_req;
		};
	}
}