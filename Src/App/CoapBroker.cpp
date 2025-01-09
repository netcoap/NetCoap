//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//


#ifdef _WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#endif

#include <chrono>
#include <coroutine>
#include <utility>
#include <iostream>
#include <memory>

#include "Toolbox/Toolbox.h"

import Coap;
import Toolbox;
import PubSub;

using namespace std;

using namespace netcoap::toolbox;
using namespace netcoap::coap;
using namespace netcoap::pubsub;

void runBroker() {
	JsonPropTree cfg;
	cfg.fromJsonFile("../ConfigFile/NetCoap.cfg"); // "C:\\Projects\\NetCoap\\ConfigFile\\NetCoap.cfg");
	UdpServerDtlsIo dtls;
	Broker broker(cfg, dtls);
	broker.run();
}


int main() {
	runBroker();
}