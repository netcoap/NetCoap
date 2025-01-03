//
// Copyright (c) 2024-2025 Sonny Hoang <shoang@netcoap.net>
//
// NetCoap is free software; you can redistribute it and/or modify
// it under the terms of the Apache License, Version 2.0. See LICENSE-2.0.txt for details.
//

module;

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <fstream>
#include <string>
#include <exception>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <functional>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#include <openssl/x509.h>

#include "Toolbox/Toolbox.h"
#include "Coap/Coap.h"

export module Coap:UdpServerDtlsIo;

import Toolbox;
import :IoSession;

using namespace std;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace coap {

		class Buffer {
		public:

			Buffer() : m_readBuf(IO_BUFFER_SIZE, 0) {}

			size_t getTotalEle() {
				return (m_tail - m_head);
			}

			void insert(char* s, size_t len) {

				size_t totalEle = m_tail - m_head;
				if ((totalEle + len) > m_readBuf.size()) {
					size_t spaceNeeded = len - totalEle + 1;
					if (spaceNeeded < IO_BUFFER_INCREMENTAL) {
						spaceNeeded = IO_BUFFER_INCREMENTAL;
					}
					m_readBuf.reserve(spaceNeeded);
				}

				if ((m_readBuf.size() - m_tail) < len) {
					memcpy(&m_readBuf[0], &m_readBuf[m_head], totalEle);
					m_head = 0;
					m_tail = totalEle;
				}

				memcpy(&m_readBuf[m_tail], s, len);
				m_tail += len;
			}

			inline char* get(size_t &len) {
				size_t totalEle = m_tail - m_head;
				if (len > totalEle) {
					len = totalEle;
				}

				size_t retFront = m_head;
				m_head += len;

				return &m_readBuf[retFront];
			}

		private:

			string m_readBuf;
			size_t m_head = 0;
			size_t m_tail = 0;
		};

		export class IoContext {
		public:
			enum class CONNECTION_STATE { LISTEN, ACCEPT, COMPLETED };

			IoContext() {}

			~IoContext() {
				if (ssl) {
					SSL_free(ssl);
				}
			}

			CONNECTION_STATE connectionState = CONNECTION_STATE::LISTEN;
			SSL* ssl = nullptr;
			SyncQ<shared_ptr<IoBuf>> inpQ;
			BIO_ADDR* bioAddr;
			Buffer inpBuf;
			IpAddress clientAddr;
		};

		export class UdpServerDtlsIo : public IoSession {
		public:

			UdpServerDtlsIo(const UdpServerDtlsIo&) = delete;
			UdpServerDtlsIo& operator=(const UdpServerDtlsIo&) = delete;

			UdpServerDtlsIo() {}

			virtual ~UdpServerDtlsIo() {
				if (m_sslCtx) {
					SSL_CTX_free(m_sslCtx);
				}
			}

			int write(shared_ptr<IoBuf> buf, void* ctx) {
				m_outpQ.push(buf);
				return (int) buf->buf.length();
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

					IoContext* ioContext = m_ioContextMap[buf->clientAddr.toString()].get();

					int retryCount = 0;
					size_t totalSent = 0;
					size_t bytesSent;
					int errNo = 0;
					while ((totalSent < buf->buf.length()) && (retryCount < MAX_RETRIES)) {
						errNo = SSL_write_ex(
							ioContext->ssl,
							(buf->buf.data() + totalSent),
							(buf->buf.length() - totalSent),
							&bytesSent);

						if (errNo > 0) {
							totalSent += bytesSent;
							retryCount = 0;
							continue;
						}

						errNo = SSL_get_error(ioContext->ssl, errNo);
						if ((errNo != SSL_ERROR_WANT_READ) && (errNo != SSL_ERROR_WANT_WRITE)) {
							retryCount++;
						}

						continue;
					}

					if (totalSent != buf->buf.length()) {
						LIB_MSG_ERR("Unable to send message to {} cause err {}\n", buf->clientAddr.toString(), errNo);
						m_outpQ.pushFront(buf);
					}
				}
			}

			bool disconnect(IpAddress ipAddr) {
				m_ioContextMap.erase(ipAddr.toString());

				return true;
			}

			InpOutpResponse* getData() {

				IoContext* ioContext = nullptr; int ret;
				IpAddress clientAddr; socklen_t clientLen = sizeof(clientAddr);
				m_ioResp.reset();

				m_mon.monitorAll();

				if (!m_mon.getReadState(*m_sock.get())->isReadable()) {
					m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NONE;
					return &m_ioResp;
				}

				shared_ptr<IoBuf> ioBuf(new IoBuf());
				ioBuf->buf.resize(COAP_MAX_RX_SIZE);
				sockaddr_in6 src_addr;
				socklen_t addr_len = sizeof(src_addr);

				int bytesRcv = recvfrom(m_sock->getSocket(), (char*)ioBuf->buf.data(), (int)ioBuf->buf.length(),
					0, (struct sockaddr*)&clientAddr, &clientLen);

				if (bytesRcv < 0) {
					int errNo;
#ifdef _WIN32
					errNo = WSAGetLastError();
#else
					errNo = errno;
#endif

#ifdef _WIN32
					if (errNo == WSAEWOULDBLOCK) {
#else
					if ((errNo == EAGAIN) || (errNo == EWOULDBLOCK)) {
#endif
						m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NONE;
						return &m_ioResp;
					}

					LIB_MSG_ERR("Unable to receive message cause err {}\n", errNo);

					m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NONE;
					return &m_ioResp;
				}
				else if (!bytesRcv) {
					m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NONE;
					return &m_ioResp;
				}

				string key = clientAddr.toString();
				if (m_ioContextMap.find(key) == m_ioContextMap.end()) {
					unique_ptr<IoContext> ptr = make_unique<IoContext>();
					ioContext = ptr.get();
					m_ioContextMap[key] = std::move(ptr);
				}
				else {
					ioContext = m_ioContextMap[key].get();
				}

				ioContext->clientAddr = clientAddr;
				ioContext->inpBuf.insert(ioBuf->buf.data(), bytesRcv);

				switch (ioContext->connectionState) {

					case IoContext::CONNECTION_STATE::LISTEN:
						{
							if (ioContext->ssl == nullptr) {
								SSL* ssl = SSL_new(m_sslCtx);
								if (!ssl) {
									LIB_MSG_ERR_THROW_EX("Unable to get new client with SSL_new");
								}

								BIO* rbio = BIO_new(m_methUdpRcv);
								BIO* wbio = BIO_new_dgram((int)m_sock->getSocket(), BIO_NOCLOSE);
								BIO_set_data(rbio, ioContext);

								SSL_set_bio(ssl, rbio, wbio);

								ioContext->ssl = ssl;

								ioContext->bioAddr = BIO_ADDR_new();

								if (BIO_dgram_set_peer(SSL_get_wbio(ioContext->ssl), (struct sockaddr*)&clientAddr) <= 0) {
									LIB_MSG_ERR("Failed to set peer address");
								}
							}

							ret = DTLSv1_listen(ioContext->ssl, ioContext->bioAddr);
							if (ret > 0) {
								ioContext->connectionState = IoContext::CONNECTION_STATE::ACCEPT;
							}
							else if (ret < 0) {
								ret = SSL_get_error(ioContext->ssl, ret);
								if (ret != SSL_ERROR_WANT_READ && ret != SSL_ERROR_WANT_WRITE) {
									LIB_MSG_WARN("Error during DTLSv1_Listen, drop connection to {} with err {}",
										clientAddr.toString(), ret);
									m_ioContextMap.erase(key);
								}
							}

							m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NONE;
						}

					break;

					case IoContext::CONNECTION_STATE::ACCEPT: {

						ret = SSL_accept(ioContext->ssl);
						if (ret > 0) {
							ioContext->connectionState = IoContext::CONNECTION_STATE::COMPLETED;
							m_ioResp.clientAddr = clientAddr;
							m_ioResp.inpQ = &ioContext->inpQ;

							m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NEW_CONNECTION;
						}
						else {
							ret = SSL_get_error(ioContext->ssl, ret);
							if (ret != SSL_ERROR_WANT_READ && ret != SSL_ERROR_WANT_WRITE) {
								LIB_MSG_WARN("Error during SSL_accept, drop connection to {}", clientAddr.toString());
								m_ioContextMap.erase(key);
							}

							m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NONE;
						}
					}
					break;

					default:
						
						ioBuf->clientAddr = clientAddr;

						size_t readBytes = 0;
						ret = SSL_read_ex(ioContext->ssl, ioBuf->buf.data(), bytesRcv, &readBytes);
						if (ret <= 0) {
							m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::NONE;

							ret = SSL_get_error(ioContext->ssl, ret);
							LIB_MSG_WARN("Unable to read from {} with err {}", clientAddr.toString(), ret);
						}
						else {
							ioBuf->buf.resize(readBytes);
							m_ioResp.clientAddr = clientAddr;
							m_ioResp.statusRet = ServerIoResponse::STATUS_RETURN::CLIENT_HAS_DATA;
							ioContext->inpQ.push(ioBuf);
						}
					break;
				}

				return &m_ioResp;
			}

			static long udpBioCtrl(BIO* bio, int cmd, long num, void* ptr) {
				IoContext* ioContext = (IoContext*) BIO_get_data(bio);

				switch (cmd) {

					case BIO_CTRL_DGRAM_GET_PEER:
						if (ptr) {
							memcpy(ptr, &ioContext->clientAddr, sizeof(IpAddress));

							return 1;
						}

						return 0;

					case BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT:
						return 1;

					default:
						LIB_MSG_WARN("Not support udpBioCtrl cmd: {}", cmd);
						return 0;
				}
			}

			static int udpRcv(BIO* bio, char* buf, int len) {
				IoContext* ioContext = (IoContext*) BIO_get_data(bio);
				if (!ioContext) {
					return -1;
				}

				size_t cpDataLen = len;
				char* inpBufPtr = ioContext->inpBuf.get(cpDataLen);
				if (cpDataLen > 0) {
					memcpy(buf, inpBufPtr, cpDataLen);

					return cpDataLen;
				}
				else {
					BIO_set_retry_read(bio);
					return 0;
				}

				return 0;
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

				m_sslCtx = SSL_CTX_new(DTLS_server_method());
				if (!m_sslCtx) {
					LIB_MSG_ERR_THROW_EX("Unable to initialize in perform SSL_CTX_new");
				}
				
				SSL_CTX_set_max_proto_version(m_sslCtx, TLS1_2_VERSION);

				// Clients cannot resume previous SSL/TLS sessions,
				// so every new connection will require a full handshake
				SSL_CTX_set_session_cache_mode(m_sslCtx, SSL_SESS_CACHE_OFF);

				// Set SSL context to require a client certificate and only request it once - SSL_VERIFY_FAIL_IF_NO_PEER_CERT
				SSL_CTX_set_verify(m_sslCtx,
					SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
					dtlsVerifyCb);

				// Enabling read ahead allows OpenSSL to read and buffer more data
				// than it immediately needs for the current SSL/TLS record
				SSL_CTX_set_read_ahead(m_sslCtx, 1);

				SSL_CTX_set_cookie_generate_cb(m_sslCtx, generateCookieCb);
				SSL_CTX_set_cookie_verify_cb(m_sslCtx, verifyCookieCb);

				string caCertificatePath = cfg.get<string>("coap.caCertificate");
				string serverCertificatePath = cfg.get<string>("coap.serverCertificate");
				string serverPrivateKeyPath = cfg.get<string>("coap.serverPrivateKey");

				BIO* bio = BIO_new_file(caCertificatePath.data(), "r");
				if (!bio) {
					LIB_MSG_ERR_THROW_EX("Unable to open file {}", caCertificatePath);
				}
				X509* caCert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
				BIO_free(bio);
				if (!caCert) {
					LIB_MSG_ERR_THROW_EX("Unable to load PEM_read_X509 {}", caCertificatePath);
				}

				if (!SSL_CTX_use_certificate_file(m_sslCtx, serverCertificatePath.c_str(), SSL_FILETYPE_PEM)) {
					LIB_MSG_ERR_THROW_EX("Invalid certificate file {}", serverCertificatePath);
				}

				if (!SSL_CTX_use_PrivateKey_file(m_sslCtx, serverPrivateKeyPath.c_str(), SSL_FILETYPE_PEM)) {
					LIB_MSG_ERR_THROW_EX("Invalid private key file {}", serverPrivateKeyPath);
				}
				if (!SSL_CTX_check_private_key(m_sslCtx)) {
					LIB_MSG_ERR_THROW_EX("Invalid SSL_CTX_check_private_key file {}", serverPrivateKeyPath);
				}

				SSL_CTX_set_ex_data(m_sslCtx, SSL_COOKIE_INIT_IDX, &m_cookieInit);
				SSL_CTX_set_ex_data(m_sslCtx, SSL_COOKIE_SECRET_IDX, &m_cookieSecret);
				SSL_CTX_set_ex_data(m_sslCtx, SSL_CA_CERT_IDX, caCert);

				string serverAddress = cfg.get<string>("coap.serverIpAddress");
				int serverPort = cfg.get<int>("coap.serverPort");
				IpAddress ipAddress(serverAddress, serverPort);

				m_methUdpRcv = BIO_meth_new(BIO_TYPE_DGRAM, "NetCoapUdp");
				BIO_meth_set_read(m_methUdpRcv, udpRcv);
				BIO_meth_set_ctrl(m_methUdpRcv, udpBioCtrl);

				m_sock = make_unique<Socket>(ipAddress.getFamily(), Socket::SocketType::UDP);
				m_sock->bind(ipAddress);
				m_sock->setNonBlocking();
				m_mon.addRead(*m_sock.get());
				m_mon.addWrite(*m_sock.get());
			}

		private:

			static long udpBioCtrl(BIO* bio, int cmd, long num, void* ptr) {
				IoContext* ioContext = (IoContext*)BIO_get_data(bio);

				switch (cmd) {

				case BIO_CTRL_DGRAM_GET_PEER:
					if (ptr) {
						memcpy(ptr, &ioContext->clientAddr, sizeof(IpAddress));

						return 1;
					}

					return 0;

				case BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT:
					return 1;

				default:
					LIB_MSG_WARN("Not support udpBioCtrl cmd: {}", cmd);
					return 0;
				}
			}

			static int udpRcv(BIO* bio, char* buf, int len) {
				IoContext* ioContext = (IoContext*)BIO_get_data(bio);
				if (!ioContext) {
					return -1;
				}

				size_t cpDataLen = len;
				char* inpBufPtr = ioContext->inpBuf.get(cpDataLen);
				if (cpDataLen > 0) {
					memcpy(buf, inpBufPtr, cpDataLen);

					return cpDataLen;
				}
				else {
					BIO_set_retry_read(bio);
					return 0;
				}

				return 0;
			}

			static int verifyCert(X509* cert, X509* caCert) {

				X509_STORE* store = X509_STORE_new();
				if (!store) {
					LIB_MSG_ERR("Failed to create X509_STORE");
					return 0;
				}

				if (X509_STORE_add_cert(store, caCert) != 1) {
					LIB_MSG_ERR_THROW_EX("Failed to add CA certificate to store");
					return 0;
				}

				X509_STORE_CTX* ctx = X509_STORE_CTX_new();
				if (!ctx) {
					LIB_MSG_ERR_THROW_EX("Failed to create X509_STORE_CTX");
					return 0;
				}
				X509_STORE_CTX_init(ctx, store, cert, NULL);

				int result = X509_verify_cert(ctx);

				if (result == 1) {
					return 1;
				}
				else {
					LIB_MSG_ERR("Certificate verification error {}",
						X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)));
					return 0;
				}
				X509_STORE_CTX_free(ctx);
				X509_STORE_free(store);
			}

			static int dtlsVerifyCb(int ok, X509_STORE_CTX* ctx) {

				// Retrieve the current certificate being processed
				X509* cert = X509_STORE_CTX_get_current_cert(ctx);

				// Retrieve the SSL object from the X509_STORE_CTX
				SSL* ssl = (SSL*) X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
				if (!ssl) {
					LIB_MSG_ERR_THROW_EX("Failed to retrieve SSL object");
					return 0; // Fail verification if SSL object is not found
				}
				// Retrieve the SSL context (SSL_CTX) from the SSL object
				SSL_CTX* sslCtx = SSL_get_SSL_CTX(ssl);
				X509* ca_cert = (X509*) SSL_CTX_get_ex_data(sslCtx, SSL_CA_CERT_IDX);

				return verifyCert(cert, ca_cert);
			}

			static int generateCookieCb(SSL* ssl, unsigned char* cookie, unsigned int* cookieLen) {

				unsigned char result[EVP_MAX_MD_SIZE];
				unsigned int resultLen = 0;
				IpAddress peer;

				SSL_CTX* sslCtx = SSL_get_SSL_CTX(ssl);
				bool* init = (bool*) SSL_CTX_get_ex_data(sslCtx, SSL_COOKIE_INIT_IDX);
				unsigned char* cookies = (unsigned char*) SSL_CTX_get_ex_data(sslCtx, SSL_COOKIE_SECRET_IDX);

				/* Initialize a random secret */
				if (!*init)
				{
					if (!RAND_bytes(cookies, COOKIE_SECRET_LENGTH))
					{
						return 0;
					}
					*init = true;
				}

				/* Read peer information */
				memset(&peer, 0, sizeof(peer));
				BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

				/* Calculate HMAC of buffer using the secret */
				HMAC(EVP_sha256(), (const void*) cookies, COOKIE_SECRET_LENGTH,
					(const unsigned char*) &peer, peer.getAddressLen(), result, &resultLen);

				memcpy(cookie, result, resultLen);
				*cookieLen = resultLen;

				//LIB_MSG_DEBUG_HEX_DUMP("Generate Cookie:", (const char*) cookie, resultLen);

				return 1;
			}

			static int verifyCookieCb(SSL* ssl, const unsigned char* cookie, unsigned int cookieLen) {

				unsigned char result[EVP_MAX_MD_SIZE];
				unsigned int resultLen = 0;
				IpAddress peer;

				SSL_CTX* ssl_ctx = SSL_get_SSL_CTX(ssl);
				bool* init = (bool*)SSL_CTX_get_ex_data(ssl_ctx, SSL_COOKIE_INIT_IDX);
				char* cookies = (char*)SSL_CTX_get_ex_data(ssl_ctx, SSL_COOKIE_SECRET_IDX);

				/* If secret isn't initialized yet, the cookie can't be valid */
				if (!*init) {
					LIB_MSG_ERR_THROW_EX("Secret isn't initialized yet...");
					return 0;
				}

				memset(&peer, 0, sizeof(peer));
				/* Read peer information */
				BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

				/* Calculate HMAC of buffer using the secret */
				HMAC(EVP_sha256(), (const void*) cookies, COOKIE_SECRET_LENGTH,
					(const unsigned char*) &peer, peer.getAddressLen(), result, &resultLen);

				//LIB_MSG_DEBUG_HEX_DUMP("Verify Cookie:", (const char*) result, resultLen);

				if (cookieLen == resultLen && memcmp(result, cookie, resultLen) == 0) {
					return 1;
				}

				return 0;
			}

			static const uint8_t SSL_COOKIE_INIT_IDX = 0;
			static const uint8_t SSL_COOKIE_SECRET_IDX = 1;
			static const uint8_t SSL_CA_CERT_IDX = 2;

			bool m_cookieInit = false;
			static const uint8_t COOKIE_SECRET_LENGTH = 16;
			unsigned char m_cookieSecret[COOKIE_SECRET_LENGTH];

			SocketMonitor m_mon{ chrono::microseconds(100) };
			SSL_CTX* m_sslCtx = nullptr;
			unique_ptr<Socket> m_sock = nullptr;
			SyncQ<shared_ptr<IoBuf>> m_outpQ;
			ServerIoResponse m_ioResp;
			unordered_map<string, unique_ptr<IoContext>> m_ioContextMap;
			BIO_METHOD* m_methUdpRcv;
		};
	}
}
