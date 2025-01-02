//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//


#ifdef _WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <csignal>
#else
#include <signal.h>
#endif

#include <algorithm>
#include <iostream>
#include <fstream>
#include <coroutine>
#include <chrono>
#include <thread>
#include <memory>
#include <functional>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>

#include "Toolbox/Toolbox.h"

import Coap;
import Toolbox;
import PubSub;

using namespace std;
using namespace chrono;

using namespace netcoap::toolbox;
using namespace netcoap::coap;
using namespace netcoap::pubsub;

string g_NetCoapCONFIG_FILE = "../ConfigFile/NetCoap.cfg";

Client* g_client = nullptr;
string g_cfgUriPath = "";
string g_dataUriPath = "/www/topic/ps/data";
string g_topicName = "Weather";
string g_topicUriPath = "/www/topic/ps";
UdpClientDtlsIo g_dtls;
JsonPropTree g_cfg;


string g_stopTestMsg = "<--- Press ^c to stop testing --->\n";

void tstPublish() {

	g_cfg.fromJsonFile(g_NetCoapCONFIG_FILE);
	g_client = new Client(g_cfg, g_dtls);
	bool connected = g_client->connect();

	if (!connected) {
		exit(1);
	}

	string s = "0123456789abcdefghijklmnopqrstuvwxyz0123456789";

	for (int i = 0; i < 10; i++) {
		shared_ptr<string> data(new string(70000, 0));
		Helper::syncOut() << i << " Publish data length: " << data->size() << "\n"; // << "; data: " << *data << "\n";
		g_client->publish(g_dataUriPath, data, Message::CONTENT_FORMAT::TEXT_PLAIN, true);
		reverse(s.begin(), s.end());
	}
}

static void
interruptCb(int signum) {
	g_client->disconnect();
	while (g_client->getState() != Client::STATE::NONE) {}

	exit(signum);
}

#ifndef _WIN32
struct sigaction sa;
#endif

int main() {

#ifdef _WIN32
	signal(SIGINT, interruptCb);
#else
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = interruptCb;
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	/* So we do not exit on a SIGPIPE */
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
#endif

	tstPublish();

	char ch;
	Helper::syncOut() << g_stopTestMsg;
	cin >> ch;
}
