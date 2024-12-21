//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <iostream>
#include <memory>

export module PubSub:BrokerIf;

import Coap;

using namespace std;
using namespace netcoap::coap;

namespace netcoap {
	namespace pubsub {

		export class Session;

		export class BrokerIf {
		public:

			BrokerIf() {}
			virtual ~BrokerIf() {}

			virtual void runSession(Session* sess) = 0;
			virtual void msgRcv(shared_ptr<Message> msg) = 0;
		};
	}
}