
<h2>NetCoap is a C++20 with modules implementation for the secure UDP Constrained Application Protocol (CoAP) Publish/Subscribe. CoAP Pub/Sub is described in the draft-ietf-core-coap-pubsub-15. NetCoap is also hosted in https://netcoap.net</h2>

<h3>The following RFCs are supported:</h3>

1. [<b>draft-ietf-core-coap-pubsub-15: A publish-subscribe architecture for the Constrained Application Protocol (CoAP)</b>](https://www.ietf.org/archive/id/draft-ietf-core-coap-pubsub-15.txt)
2. [<b>RFC7252: The Constrained Application Protocol (CoAP)</b>](https://www.rfc-editor.org/rfc/rfc7252)
3. [<b>RFC7641: Observing Resources in the Constrained Application Protocol (CoAP)</b>](https://www.rfc-editor.org/rfc/rfc7641)
4. [<b>RFC7959: Block-Wise Transfers in the Constrained Application Protocol (CoAP)</b>](https://www.rfc-editor.org/rfc/rfc7959)
5. [<b>RFC8949: Concise Binary Object Representation (CBOR)</b>](https://www.rfc-editor.org/rfc/rfc8949.html)

<h3>Installation</h3>

<p><b>For Windows and Linux:</b></p>
  <p>Install AND Build OpenSSL 3.4.0 22 Oct 2024</p>

  <p><b>For Linux:</b></p>

* sudo apt install clang # Version 18. Note that Gnu CC does not support fully support module definition
* sudo apt install cmake # Version 3.31.3
* sudo apt install ninja # Version 1.11.1

<p><b>For Windows:</b></p>

* Install Microsoft Visual Studio Community 2022 Version 17.12.2

<h3>Generate Self-signed Certificate</h3>

  <p><b>Go to directory Certificate and do the following commands:</b></p>

	openssl genpkey -algorithm RSA -out caPrivateKey.pem -pkeyopt rsa_keygen_bits:4096
	openssl req -x509 -new -key caPrivateKey.pem -days 3650 -out caCertificate.pem

	openssl genpkey -algorithm RSA -out serverPrivateKey.pem -pkeyopt rsa_keygen_bits:2048
	openssl req -new -key serverPrivateKey.pem -out serverCsr.pem
	openssl x509 -req -in serverCsr.pem -CA caCertificate.pem -CAkey caPrivateKey.pem -CAcreateserial -out serverCertificate.pem -days 365

	openssl genpkey -algorithm RSA -out clientPrivateKey.pem -pkeyopt rsa_keygen_bits:2048
	openssl req -new -key clientPrivateKey.pem -out clientCsr.pem
	openssl x509 -req -in clientCsr.pem -CA caCertificate.pem -CAkey caPrivateKey.pem -CAcreateserial -out clientCertificate.pem -days 365

	openssl verify -CAfile caCertificate.pem serverCertificate.pem
	openssl verify -CAfile caCertificate.pem clientCertificate.pem

<h3>Configure NetCoap</h3>

* Edit CoapBroker.cpp, CoapSubscriber.cpp, CoapPublisher.cpp to change g_NetCoapCONFIG_FILE to point to NetCoap.cfg configuration file
* Make sure configuration directory path specified in the NetCoap.cfg points to the correct certificate files

<h3>Building</h3>

<p><b>For Linux:</b></p>
	
* edit CMakeLists.txt in the top directory and change to point to OpenSSL library e.g: link_directories("~/Projects/OpenSSL/Dist-3.4.0/lib64")
* Goto top directory and mkdir build; cd build
* cmake -G Ninja ..
* cmake --build . -v

<p><b>For Windows:</b></p>

* Open Win/NetCoap.sln and change CoapBroker, CoapPublisher, CoapSubscriber projects to point to OpenSSL library under Linker -> Additional Library Directories
* Build solution

<h3>Execution</h3>

<p><b>For Linux: Under build directory should contain CoapBroker, CoapPublisher and CoapSubscriber</b></p>

<p><b>For Windows and Linux</b></p>

* Execute 1st: CoapBroker
* Execute 2nd: CoapSubscriber
* Execute 3rd: CoapPublisher

<p><b>NetCoap has been successfully tested under Windows and Linux 64 bits</b></p>
