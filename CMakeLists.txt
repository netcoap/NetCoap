cmake_minimum_required(VERSION 3.31.2)

project(NetCoap VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_EXTENSION ".ixx")

set(CMAKE_CXX_COMPILER /usr/bin/clang++)

add_subdirectory(Src/Toolbox)
add_subdirectory(Src/Coap)
add_subdirectory(Src/PubSub)

target_link_libraries(Coap PRIVATE Toolbox)
target_link_libraries(PubSub PRIVATE Toolbox Coap)

link_directories("~/Projects/OpenSSL/Dist-3.4.0/lib64")

add_executable(CoapBroker Src/App/CoapBroker.cpp)
add_executable(CoapSubscriber Src/App/CoapSubscriber.cpp)
add_executable(CoapPublisher Src/App/CoapPublisher.cpp)

target_include_directories(CoapBroker PRIVATE
    ..
    ../Toolbox
    ../Coap)

target_include_directories(CoapSubscriber PRIVATE
    ..
    ../Toolbox
    ../Coap)

target_include_directories(CoapPublisher PRIVATE
    ..
    ../Toolbox
    ../Coap)

target_link_libraries(CoapBroker PRIVATE Toolbox Coap PubSub crypto ssl)
target_link_libraries(CoapSubscriber PRIVATE Toolbox Coap PubSub crypto ssl)
target_link_libraries(CoapPublisher PRIVATE Toolbox Coap PubSub crypto ssl)
