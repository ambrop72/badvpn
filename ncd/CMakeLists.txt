add_executable(badvpn-ncd
    ncd.c
    NCDValue.c
    NCDModule.c
    NCDIfConfig.c
    NCDInterfaceMonitor.c
    modules/command_template.c
    modules/var.c
    modules/list.c
    modules/depend.c
    modules/concat.c
    modules/if.c
    modules/strcmp.c
    modules/logical.c
    modules/ip_in_network.c
    modules/run.c
    modules/net_backend_physical.c
    modules/net_backend_badvpn.c
    modules/net_dns.c
    modules/net_iptables.c
    modules/net_ipv4_addr.c
    modules/net_ipv4_route.c
    modules/net_ipv4_dhcp.c
)
target_link_libraries(badvpn-ncd system dhcpclient ncdconfig process)

install(
    TARGETS badvpn-ncd
    RUNTIME DESTINATION bin
)
