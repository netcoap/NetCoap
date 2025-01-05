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
#include <random>
#include <string.h>

#include "Toolbox/Toolbox.h"

import Coap;
import Toolbox;
import PubSub;

using namespace std;
using namespace chrono;

using namespace netcoap::toolbox;
using namespace netcoap::coap;
using namespace netcoap::pubsub;

string g_NetCoapCONFIG_FILE = "../ConfigFile/NetCoap.cfg"; // "C:\\Projects\\NetCoap\\ConfigFile\\NetCoap.cfg";

Client* g_client = nullptr;
string g_cfgUriPath = "";
string g_dataUriPath = "/www/topic/ps/weather";
string g_topicName = "Weather";
string g_topicUriPath = "/www/topic/ps";
UdpClientDtlsIo g_dtls;
JsonPropTree g_cfg;

string g_stopTestMsg = "<--- Press ^c to stop testing --->\n";

void connect() {

	g_cfg.fromJsonFile(g_NetCoapCONFIG_FILE);
	g_client = new Client(g_cfg, g_dtls);
	bool connected = g_client->connect();

	if (!connected) {
		exit(1);
	}
}

void tstPublish() {

	JsonPropTree jsonPropTree;

	for (int i = 0; i < 10; i++) {
		random_device rd;
		mt19937 gen(rd());

		uniform_int_distribution<int> humidityDist(50, 70);
		uniform_real_distribution<float> temperatureDist(70.0, 73.0);

		int humidity = humidityDist(gen);
		float temperature = temperatureDist(gen);

		// ============================ Publish Temperature ======================

		string jsonTemperature =
			string("{") +
			"\"" + "Title:" + "\"" + ':' + "\"Weather\"" + "," +
			"\"" + "temperature" + "\"" + ':' + to_string(temperature) +
			string("}");

		shared_ptr<string> temperatureDat(new string());
		jsonPropTree.fromJsonStr(jsonTemperature);
		jsonPropTree.toCborStr(*temperatureDat);

		g_client->publish(
			g_dataUriPath, temperatureDat, Message::CONTENT_FORMAT::APP_CBOR,
			true, "temperature");

		// ============================ Publish Humidity ==========================

		string jsonHumidity =
			string("{") +
			"\"" + "Title:" + "\"" + ':' + "\"Weather\"" + "," +
			"\"" + "humidity" + "\"" + ':' + to_string(humidity) +
			string("}");

		shared_ptr<string> humidityDat(new string());
		jsonPropTree.fromJsonStr(jsonHumidity);
		jsonPropTree.toCborStr(*humidityDat);

		g_client->publish(
			g_dataUriPath, humidityDat, Message::CONTENT_FORMAT::APP_CBOR,
			true, "humidity");
	}
}

void subscribeCb(
	Client::STATUS status,
	const shared_ptr<Message> respMsg) {

	shared_ptr<string> payLoad = respMsg->getPayload();
	JsonPropTree jsonPropTree;
	jsonPropTree.fromCborStr(*payLoad);

	int humidity = jsonPropTree.get<int>("humidity");
	Helper::syncOut() << "Humidity: " << humidity << "\n";
}

void tstSubscriber() {

	g_client->subscribe(g_dataUriPath, subscribeCb, "humidity");
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

	connect();

	tstSubscriber();
	tstPublish();

	char ch;
	Helper::syncOut() << g_stopTestMsg;
	cin >> ch;
}
