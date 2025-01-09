
add_library(Toolbox STATIC)

# Specify source files
target_sources(Toolbox PUBLIC FILE_SET CXX_MODULES FILES
    Helper.ixx
    LogMessage.ixx
    JsonValue.ixx
    CoroPool.ixx
    JsonPropTree.ixx
    SocketMonitor.ixx
    Socket.ixx
    SyncQ.ixx
    Toolbox.ixx
    Ssl.ixx)

target_include_directories(Toolbox PUBLIC ..)