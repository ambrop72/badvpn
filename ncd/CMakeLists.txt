add_executable(badvpn-ncd
    ncd.c
    NCDValue.c
    NCDModule.c
    NCDIfConfig.c
    NCDInterfaceMonitor.c
    modules/var.c
    modules/list.c
    modules/depend.c
    modules/net_backend_physical.c
    modules/net_backend_badvpn.c
    modules/net_dns.c
    modules/net_ipv4_addr.c
    modules/net_ipv4_route.c
    modules/net_ipv4_dhcp.c
)
target_link_libraries(badvpn-ncd system dhcpclient ncdconfig)

install(
    TARGETS badvpn-ncd
    RUNTIME DESTINATION bin
)
