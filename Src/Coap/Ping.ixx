//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <iostream>
#include <chrono>

#include "Coap/Coap.h"

export module Coap:Ping;

using namespace std;
using namespace chrono;

namespace netcoap {
	namespace coap {

		export class Ping {
		public:

			inline void setKeepAlive(int keepAlive_sec) {
				m_keepAlive_sec = keepAlive_sec;
			}

			inline void stampRcvMsg() {
				m_lastRcvMsg = high_resolution_clock::now();
				m_time2CkPing = true;
			}

			inline bool isTime4Ping() {
				time_point now = high_resolution_clock::now();;
				duration durClientIdle_sec = duration_cast<seconds>(now - m_lastRcvMsg);

				if (!m_time2CkPing) {
					return false;
				}

				if (durClientIdle_sec.count() > m_keepAlive_sec) {
					m_time2CkPing = false;
					return true;
				}

				return false;
			}

			inline bool isClientAlive() {
				time_point now = high_resolution_clock::now();;
				duration durClientIdle_sec = duration_cast<seconds>(now - m_lastRcvMsg);

				if (durClientIdle_sec.count() >
					(m_keepAlive_sec + MAX_TRANSMIT_WAIT_sec)) {
					return false;
				}

				return true;
			}

		private:

			time_point<high_resolution_clock> m_lastRcvMsg = high_resolution_clock::now();
			bool m_time2CkPing = true;
			int m_keepAlive_sec = EXCHANGE_LIFETIME_sec;
		};
	}
}