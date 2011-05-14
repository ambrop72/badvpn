add_library(flowextra
    DatagramSocketSource.c
    DatagramSocketSink.c
    StreamSocketSource.c
    StreamSocketSink.c
    PacketPassInactivityMonitor.c
    KeepaliveIO.c
)
target_link_libraries(flowextra flow system security)
