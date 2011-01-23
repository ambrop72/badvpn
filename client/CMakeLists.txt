add_executable(badvpn-client
    client.c
    StreamPeerIO.c
    DatagramPeerIO.c
    PasswordListener.c
    DataProto.c
    PasswordSender.c
    FrameDecider.c
    DPRelay.c
)
target_link_libraries(badvpn-client system flow tuntap server_conection security ${NSPR_LIBRARIES} ${NSS_LIBRARIES})

install(
    TARGETS badvpn-client
    RUNTIME DESTINATION bin
)
