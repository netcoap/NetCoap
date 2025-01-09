//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <openssl/ssl.h>

#include "Toolbox/Toolbox.h"

export module Toolbox:Ssl;

import :LogMessage;

using namespace std;

namespace netcoap {
	namespace toolbox {

		export class Ssl {
		public:

			Ssl() {}

			~Ssl() {
				if (m_ssl) {
					SSL_free(m_ssl);
				}
			}

			Ssl(SSL_CTX* sslCtx) {
				m_ssl = SSL_new(sslCtx);
				if (!m_ssl) {
					LIB_MSG_ERR("Error create SSL with SSL_new");
				}
			}

			inline SSL* getSsl() {
				return m_ssl;
			}

			int read(string& buf) {

				size_t readBytes = 0;
				int ret = SSL_read_ex(m_ssl, buf.data(), buf.size(), &readBytes);
				if (ret <= 0) {

					int err = SSL_get_error(m_ssl, ret);
					if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
						LIB_MSG_WARN("Error during SSL_read_ex");

						return -1;
					}

					return 0;
				}

				buf.resize(readBytes);

				return readBytes;
			}

			int write(string& buf) {
				static const int MAX_RETRIES = 3;

				int retryCount = 0;
				size_t totalSent = 0;
				size_t bytesSent;
				int errNo = 0;

				while ((totalSent < buf.length()) && (retryCount < MAX_RETRIES)) {
					errNo = SSL_write_ex(
						m_ssl,
						(buf.data() + totalSent),
						(buf.length() - totalSent),
						&bytesSent);

					if (errNo > 0) {
						totalSent += bytesSent;
						retryCount = 0;
						continue;
					}

					errNo = SSL_get_error(m_ssl, errNo);
					if ((errNo != SSL_ERROR_WANT_READ) && (errNo != SSL_ERROR_WANT_WRITE)) {
						retryCount++;
					}

					continue;
				}

				if (totalSent != buf.length()) {
					LIB_MSG_ERR("Unable to send message cause err {}\n", errNo);
					return -1;
				}
				else {
					return totalSent;
				}
			}

		private:
			SSL* m_ssl = nullptr;
		};
	}
}