//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

#pragma once

#define MSG_CODE(N)										((((N) / 100) << 5) | ((N) % 100))

#define COAP_MAX_RX_SIZE								1472 // 1500 bytes (MTU) - 20 bytes (IP Header) - 8 bytes (UDP Header)

#define ACK_TIMEOUT_sec                                 2
#define ACK_RANDOM_FACTOR                               1.5
#define MAX_RETRANSMIT                                  4
#define NSTART                                          1
#define MAX_LATENCY_sec									100
#define MAX_TRANSMIT_SPAN_sec							45
#define PROCESSING_DELAY_sec							2	

#define CACHE_TIMEOUT_sec                               (MAX_RETRANSMIT * ACK_TIMEOUT_sec)
#define EXCHANGE_LIFETIME_sec							247 // (MAX_TRANSMIT_SPAN + (2 * MAX_LATENCY) + PROCESSING_DELAY)
#define MAX_TRANSMIT_WAIT_sec 							93 // ACK_TIMEOUT* ((2 * (MAX_RETRANSMIT + 1)) - 1) * ACK_RANDOM_FACTOR

#define MAX_BLOCK_SIZE									1024
#define BLOCK_SZX										6
#define MAX_BLOCK_BYTES_XFER							1073741824 // 2**20 * 1024

#define IO_BUFFER_SIZE									4096
#define IO_BUFFER_INCREMENTAL							1024