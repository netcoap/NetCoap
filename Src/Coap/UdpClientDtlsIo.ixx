//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#include <string>
#include <exception>
#include <iostream>
#include <memory>
#include <chrono>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#include <openssl/x509.h>

#include "Coap/Coap.h"
#include "Toolbox/Toolbox.h"

export module Coap:UdpClientDtlsIo;

import Toolbox;
import :IoSession;
import :Message;

using namespace std;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace coap {

		export class UdpClientDtlsIo : public IoSession {
		public:

			UdpClientDtlsIo(const UdpClientDtlsIo&) = delete;
			UdpClientDtlsIo& operator=(const UdpClientDtlsIo&) = delete;

			UdpClientDtlsIo() {}

			virtual ~UdpClientDtlsIo() {
				if (m_ssl) {
					SSL_free(m_ssl);
				}
				if (m_sslCtx) {
					SSL_CTX_free(m_sslCtx);
				}
			}

			InpOutpResponse* getData() {
				int ret;
				IpAddress serverAddr; int serverLen = sizeof(serverAddr);

				m_mon.monitorAll();

				if (!m_mon.getReadState(*m_sock.get())->isReadable()) {
					return nullptr;
				}
				shared_ptr<IoBuf> ioBuf(new IoBuf());
				ioBuf->buf.resize(COAP_MAX_RX_SIZE);
				size_t readBytes = 0;
					
				ret = SSL_read_ex(m_ssl, ioBuf->buf.data(), COAP_MAX_RX_SIZE, &readBytes);
				if (ret <= 0) {

					unsigned long error_code = ERR_get_error();
					char error_msg[120];
					ERR_error_string_n(error_code, error_msg, sizeof(error_msg));
					printf("SSL Error: %s\n", error_msg);

					ret = SSL_get_error(m_ssl, ret);
					if (ret == SSL_ERROR_WANT_READ) {
						printf("SSL_read would block, need to wait for more data\n");
					}
					else {
						fprintf(stderr, "SSL_read failed with error %d\n", ret);
						ERR_print_errors_fp(stderr);
					}
					LIB_MSG_WARN("Unable to read from {} with err {}", serverAddr.toString(), ret);
					ERR_print_errors_fp(stderr);
				}
				else {

					ioBuf->buf.resize(readBytes);
					ioBuf->clientAddr = serverAddr;
					m_inpQ.push(ioBuf);
				}

				return nullptr;
			}

			void outDataToNet() {
				static const int MAX_RETRIES = 3;
				shared_ptr<IoBuf> buf = nullptr; bool success;

				while (true) {

					m_mon.monitorAll();

					if (!m_mon.getWriteState(*m_sock.get())->isWritable()) {
						return;
					}

					success = m_outpQ.pop(buf);
					if (!success) {
						break;
					}

					int retryCount = 0;
					size_t totalSent = 0;
					size_t bytesSent;
					int errNo = 0;
					while ((totalSent < buf->buf.length()) && (retryCount < MAX_RETRIES)) {
						errNo = SSL_write_ex(
							m_ssl,
							(buf->buf.data() + totalSent),
							(buf->buf.length() - totalSent),
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
						unsigned long error_code = ERR_get_error();
						char error_msg[120];
						ERR_error_string_n(error_code, error_msg, sizeof(error_msg));
						LIB_MSG_ERR("SSL Error: {}", error_msg);
						continue;
					}

					if (totalSent != buf->buf.length()) {
						LIB_MSG_ERR("Unable to send message cause err {}\n", errNo);
						m_outpQ.pushFront(buf);
					}
				}
			}

			int read(shared_ptr<IoBuf>& buf, void* ctx) {
				if (m_inpQ.pop(buf)) {
					return buf->buf.length();
				}
				else {
					return 0;
				}
			}

			int write(shared_ptr<IoBuf> buf, void* ctx) {
				m_outpQ.push(buf);
				return (int)buf->buf.length();
			}

			bool connect(void* ctx) {

				/* Create BIO, connect and set to already connected */
				BIO* bio = BIO_new_dgram((int) m_sock->getSocket(), BIO_NOCLOSE);
				if (!bio) {
					LIB_MSG_ERR_THROW_EX("Unable to create BIO with BIO_new_dgram");
				}

				SSL_set_bio(m_ssl, bio, bio);

				while (true) {
					int retVal = SSL_connect(m_ssl);
					if (retVal <= 0) {
						retVal = SSL_get_error(m_ssl, retVal);
						if ((retVal != SSL_ERROR_WANT_READ) && (retVal != SSL_ERROR_WANT_WRITE)) {
							LIB_MSG_ERR("Unable to connect, err {}", retVal);
#ifdef _WIN32
							LIB_MSG_ERR("Connect errno: {}", WSAGetLastError());
#else 
							LIB_MSG_ERR("Connect errno: {}", errno);
#endif
							return false;
						}

						continue;
					}

					break;
				}

				return true;
			}

			void init(JsonPropTree& cfg) {

				#ifdef _WIN32
				WSADATA wsaData;
				WSAStartup(MAKEWORD(2, 2), &wsaData);
				#endif

				if (!isInitOnce()) {

					OpenSSL_add_ssl_algorithms();
					SSL_load_error_strings();
					SSL_library_init();
					setInitOnce(true);
				}

				m_sslCtx = SSL_CTX_new(DTLS_client_method());
				if (!m_sslCtx) {
					LIB_MSG_ERR_THROW_EX("Unable to initialize in perform SSL_CTX_new");
				}

				string clientCertificatePath = cfg.get<string>("coap.clientCertificate");
				string clientPrivateKeyPath = cfg.get<string>("coap.clientPrivateKey");

				if (!SSL_CTX_use_certificate_file(m_sslCtx, clientCertificatePath.c_str(), SSL_FILETYPE_PEM)) {
					LIB_MSG_ERR_THROW_EX("Invalid certificate file {}", clientCertificatePath);
				}

				if (!SSL_CTX_use_PrivateKey_file(m_sslCtx, clientPrivateKeyPath.c_str(), SSL_FILETYPE_PEM)) {
					LIB_MSG_ERR_THROW_EX("Invalid private key file {}", clientPrivateKeyPath);
				}
				if (!SSL_CTX_check_private_key(m_sslCtx)) {
					LIB_MSG_ERR_THROW_EX("Invalid SSL_CTX_check_private_key file {}", clientPrivateKeyPath);
				}

				string serverAddress = cfg.get<string>("coap.serverIpAddress");
				int serverPort = cfg.get<int>("coap.serverPort");
				IpAddress ipAddress(serverAddress, serverPort);

				m_sock = make_unique<Socket>(ipAddress.getFamily(), Socket::SocketType::UDP);
				m_sock->connect(ipAddress);
				m_sock->setNonBlocking();
				m_mon.addRead(*m_sock.get());
				m_mon.addWrite(*m_sock.get());

				m_ssl = SSL_new(m_sslCtx);
			}

		private:

			SSL_CTX* m_sslCtx = nullptr;
			SSL* m_ssl = nullptr;
			unique_ptr<Socket> m_sock;
			SocketMonitor m_mon{ chrono::microseconds(100) };
			SyncQ<shared_ptr<IoBuf>> m_inpQ;
			SyncQ<shared_ptr<IoBuf>> m_outpQ;
		};
	}
}
