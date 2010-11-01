set(FLOW_ADDITIONAL_SOURCES)
if (NOT WIN32)
    list(APPEND FLOW_ADDITIONAL_SOURCES
        SeqPacketSocketSink.c
        SeqPacketSocketSource.c
    )
endif ()

add_library(flow
    PacketPassFairQueue.c
    PacketPassPriorityQueue.c
    PacketPassInactivityMonitor.c
    PacketPassConnector.c
    PacketRecvConnector.c
    StreamRecvConnector.c
    PacketRecvBlocker.c
    PacketRecvNotifier.c
    PacketPassNotifier.c
    PacketBuffer.c
    SinglePacketBuffer.c
    PacketCopier.c
    PacketStreamSender.c
    KeepaliveIO.c
    SCKeepaliveSource.c
    StreamSocketSource.c
    StreamSocketSink.c
    DatagramSocketSource.c
    DatagramSocketSink.c
    PacketProtoEncoder.c
    PacketProtoDecoder.c
    FragmentProtoDisassembler.c
    SPProtoEncoder.c
    SPProtoDecoder.c
    FragmentProtoAssembler.c
    DataProtoKeepaliveSource.c
    PacketProtoFlow.c
    SinglePacketSender.c
    ${FLOW_ADDITIONAL_SOURCES}
)
target_link_libraries(flow system security)
