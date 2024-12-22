
<h2>NetCoap is a C++20 with modules implementation for DTLS Secure COAP Publish-Subscribe Architecture for the Constrained Application Protocol (COAP). COAP Pub/Sub is described in draft-ietf-core-coap-pubsub-15.</h2>

<h3>The following RFCs are supported:</h3>

1. <b>draft-ietf-core-coap-pubsub-15: A publish-subscribe architecture for the Constrained Application Protocol (CoAP)</b>
2. <b>RFC7252: The Constrained Application Protocol (CoAP)</b>
3. <b>RFC7641: Observing Resources in the Constrained Application Protocol (CoAP)</b>
4. <b>RFC7959: Block-Wise Transfers in the Constrained Application Protocol (CoAP)</b>
5. <b>RFC8949: Concise Binary Object Representation (CBOR)</b>

<h3>Installation</h3>

<p><b>For Windows and Linux:</b></p>
  <p>Install AND Build OpenSSL 3.4.0 22 Oct 2024</p>

  <p><b>For Linux:</b></p>

* sudo apt install clang # Version 18
* sudo apt install cmake # Version 3.31.3
* sudo apt install ninja # Version 1.11.1

<p><b>For Windows:</b></p>

* Install Microsoft Visual Studio Community 2022 Version 17.12.2

<h3>Generate Self-signed Certificate</h3>

  <p><b>Go to directory Certificate and do the following commands:</b></p>

	openssl genpkey -algorithm RSA -out ca_private_key.pem -pkeyopt rsa_keygen_bits:4096
	openssl req -x509 -new -key ca_private_key.pem -days 3650 -out ca_certificate.pem

	openssl genpkey -algorithm RSA -out server_private_key.pem -pkeyopt rsa_keygen_bits:2048
	openssl req -new -key server_private_key.pem -out server_csr.pem
	openssl x509 -req -in server_csr.pem -CA ca_certificate.pem -CAkey ca_private_key.pem -CAcreateserial -out server_certificate.pem -days 365

	openssl genpkey -algorithm RSA -out client_private_key.pem -pkeyopt rsa_keygen_bits:2048
	openssl req -new -key client_private_key.pem -out client_csr.pem
	openssl x509 -req -in client_csr.pem -CA ca_certificate.pem -CAkey ca_private_key.pem -CAcreateserial -out client_certificate.pem -days 365

	openssl verify -CAfile ca_certificate.pem server_certificate.pem
	openssl verify -CAfile ca_certificate.pem client_certificate.pem

<h3>Building</h3>

<p><b>For Linux:</b></p>
	
* edit CMakeLists.txt in the top directory and change to point to OpenSSL library e.g: link_directories("~/Projects/OpenSSL/Dist-3.4.0/lib64")
* Goto top directory and mkdir build; cd build
* cmake -G Ninja ..
* cmake --build . -v

<p><b>Fow Windows:</b></p>

* Open Win/NetCoap.sln and change CoapBroker, CoapPublisher, CoapSubscriber projects to point to OpenSSL library under Linker -> Additional Library Directories
* Build solution

<h3>Execution</h3>

<p><b>For Linux: Under build directory should contain CoapBroker, CoapPublisher and CoapSubscriber</b></p>

<p><b>For Windows and Linux</b></p>

* Execute 1st: CoapBroker
* Execute 2nd: CoapSubscriber
* Execute 3rd: CoapPublisher
