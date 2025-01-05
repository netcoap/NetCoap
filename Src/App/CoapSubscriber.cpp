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

#include <iostream>
#include <fstream>
#include <coroutine>
#include <chrono>
#include <thread>
#include <memory>
#include <functional>
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

void createTopicCb(
	Client::STATUS status,
	const shared_ptr<Message> respMsg) {

	if (status == Client::STATUS::FAILED) {
		Helper::syncOut() << "Err in creating topic \n";
		return;
	}

	Helper::syncOut() << "***** createTopic is working... *****\n";
	Helper::syncOut() << "-----> Result from createTopic:\n";

	JsonPropTree jsonTree;
	jsonTree.fromCborStr(*respMsg->getPayload());
	jsonTree.print();

	g_cfgUriPath = respMsg->getOptionRepeatStr(Option::NUMBER::LOCATION_PATH, Option::DELIM_PATH);

	Helper::syncOut() << "cfgUriPath: " << g_cfgUriPath << "\n";
}

void tstCreateTopic() {

	g_client->createTopic(
		g_topicName, g_topicUriPath, g_dataUriPath,
		"temperature", Message::CONTENT_FORMAT::APP_JSON,
		createTopicCb);
}

void discoveryCb(
	Client::STATUS status,
	const shared_ptr<Message> respMsg) {

	Helper::syncOut() << "***** discovery is working... *****\n";
	Helper::syncOut() << "-----> Result from discovery:\n";

	if (respMsg->getPayload() != nullptr) {
		Helper::syncOut() << "Data: " + *respMsg->getPayload() + "\n";
	}
	else {
		Helper::syncOut() << "No data\n";
	}
}

void filterTopicCb(
	Client::STATUS status,
	const shared_ptr<Message> respMsg) {

	Helper::syncOut() << "***** filterTopic is working... *****\n";
	Helper::syncOut() << "-----> Result from filterTopic:\n";

	size_t start = respMsg->getPayload()->find('<');
	size_t end = respMsg->getPayload()->find('>');

	g_cfgUriPath = respMsg->getPayload()->substr(start + 1, end - start - 1);
	Helper::syncOut() << "Cfg Uri Path: " << g_cfgUriPath << "\n";
}

void tstDiscovery() {

	g_client->getAllTopicCollection(discoveryCb);
	g_client->getAllTopicCfgFromCollection(discoveryCb);
	g_client->getAllTopicData(g_topicUriPath, discoveryCb);
	g_client->getAllTopicCfg(g_topicUriPath, discoveryCb);

	JsonPropTree props;
	string json =
		string("{") +
		"\"resource-type\":" + "\"" + TopicCfgDataResource::RT_CORE_PS_CONF + "\"," +
		"\"topic-type\":" + "\"" + "temperature" + "\"" +
		string("}");
	props.fromJsonStr(json);
	g_client->getAllTopicCfgByProp(g_topicUriPath, props, filterTopicCb);
}

void getSetTopicCb(
	Client::STATUS status,
	const shared_ptr<Message> respMsg) {

	Helper::syncOut() << "***** getSetTopic is working... *****\n";
	Helper::syncOut() << "-----> Result from getSetTopic:\n";

	if (respMsg->getPayload() == nullptr) {
		Helper::syncOut() << "Null data. may be err\n";
	}
	else {
		JsonPropTree jsonTree;
		jsonTree.fromCborStr(*respMsg->getPayload());
		jsonTree.print();
	}
}

void testGetSetTopic() {

	while (g_cfgUriPath.empty()) {}

	g_client->getTopicCfg(g_cfgUriPath, getSetTopicCb);
	
	string json =
	string("{") +
	"\"" + TopicCfgResource::CONFIG_FILTER + "\"" + ':' +
	'[' +
	"\"" + TopicCfgResource::TOPIC_DATA + "\"" + ',' +
	"\"" + TopicCfgResource::TOPIC_MEDIA_TYPE + "\"" + ',' +
	"\"" + TopicCfgResource::TOPIC_TYPE + "\"" + ',' +
	"\"" + TopicCfgResource::EXPIRATION_DATE + "\"" +
	']' +
	string("}");
	
	JsonPropTree jsonTree;

	jsonTree.fromJsonStr(json);
	g_client->getTopicCfgByProp(g_cfgUriPath, jsonTree, getSetTopicCb);
	
	json =
	string("{") +
	"\"" + TopicCfgResource::MAX_SUBSCRIBERS + "\"" + ':' + "100" +
	string("}");
	jsonTree.fromJsonStr(json);
	g_client->setTopicCfgByProp(g_cfgUriPath, jsonTree, getSetTopicCb);
}


time_point g_startProcTime = high_resolution_clock::now();

void subscribeCb(
	Client::STATUS status,
	const shared_ptr<Message> respMsg) {

	shared_ptr<string> payLoad = respMsg->getPayload();
	JsonPropTree jsonPropTree;
	jsonPropTree.fromCborStr(*payLoad);

	float temperature = jsonPropTree.get<float>("temperature");
	Helper::syncOut() << "Temperature: " << fixed << setprecision(2) << temperature << "\n";
}

void tstSubscriber() {

	g_client->subscribe(g_dataUriPath, subscribeCb, "temperature");
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

	tstCreateTopic();
	tstDiscovery();
	testGetSetTopic();
	tstSubscriber();

	char ch;
	Helper::syncOut() << g_stopTestMsg;
	cin >> ch;
}
