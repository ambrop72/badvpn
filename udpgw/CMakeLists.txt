add_executable(badvpn-udpgw
    udpgw.c
)
target_link_libraries(badvpn-udpgw system flow)

install(
    TARGETS badvpn-udpgw
    RUNTIME DESTINATION bin
)
