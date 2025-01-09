add_library(PubSub STATIC)

# Specify source files
target_sources(PubSub PUBLIC FILE_SET CXX_MODULES FILES
    Broker.ixx
    Client.ixx
    PubSub.ixx
    TopicCfgResource.ixx
    BrokerIf.ixx
    CollectionResource.ixx  
    Session.ixx  
    TopicDataResource.ixx
    SessionMessage.ixx
    TokenContext.ixx)

target_include_directories(PubSub PUBLIC
    ..
    ../Toolbox
    ../Coap)
