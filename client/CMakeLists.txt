add_executable(badvpn-client
    client.c
    StreamPeerIO.c
    DatagramPeerIO.c
    PasswordListener.c
    DataProto.c
    PasswordSender.c
    FrameDecider.c
    DPRelay.c
    DPReceive.c
    FragmentProtoDisassembler.c
    FragmentProtoAssembler.c
    SPProtoEncoder.c
    SPProtoDecoder.c
    DataProtoKeepaliveSource.c
)
target_link_libraries(badvpn-client system flow flowextra tuntap server_conection security ${NSPR_LIBRARIES} ${NSS_LIBRARIES})

install(
    TARGETS badvpn-client
    RUNTIME DESTINATION bin
)
