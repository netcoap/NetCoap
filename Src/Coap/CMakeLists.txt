add_library(Coap STATIC)

# Specify source files
target_sources(Coap PUBLIC FILE_SET CXX_MODULES FILES
    Block1Xfer.ixx
    CacheMessages.ixx
    IoSession.ixx
    Option.ixx
    UdpServerDtlsIo.ixx
    Url.ixx
    Block2Xfer.ixx
    Coap.ixx
    Message.ixx
    UdpClientDtlsIo.ixx
    Ping.ixx)

target_include_directories(Coap PUBLIC
    ..
    ../Toolbox
    ../../../OpenSSL/Dist-3.4.0/include)
