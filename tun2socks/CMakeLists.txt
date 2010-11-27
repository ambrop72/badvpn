add_executable(badvpn-tun2socks
    tun2socks.c
)
target_link_libraries(badvpn-tun2socks system flow tuntap lwip socksclient)

install(
    TARGETS badvpn-tun2socks
    RUNTIME DESTINATION bin
)
