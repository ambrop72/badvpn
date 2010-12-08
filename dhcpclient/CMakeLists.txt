add_library(dhcpclientcore
    BDHCPClientCore.c
)
target_link_libraries(dhcpclientcore system flow security)

add_library(dhcpclient
    BDHCPClient.c
    DHCPIpUdpEncoder.c
    DHCPIpUdpDecoder.c
)
target_link_libraries(dhcpclient system flow dhcpclientcore)
