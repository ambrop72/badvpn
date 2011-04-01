set(NCD_ADDITIONAL_SOURCES)
if (BADVPN_USE_LINUX_RFKILL)
    list(APPEND NCD_ADDITIONAL_SOURCES
        NCDRfkillMonitor.c
        modules/net_backend_rfkill.c
    )
endif ()

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
    modules/multidepend.c
    modules/concat.c
    modules/concatlist.c
    modules/if.c
    modules/strcmp.c
    modules/logical.c
    modules/sleep.c
    modules/ip_in_network.c
    modules/run.c
    modules/net_backend_waitdevice.c
    modules/net_backend_waitlink.c
    modules/net_backend_badvpn.c
    modules/net_backend_wpa_supplicant.c
    modules/net_up.c
    modules/net_dns.c
    modules/net_iptables.c
    modules/net_ipv4_addr.c
    modules/net_ipv4_route.c
    modules/net_ipv4_dhcp.c
    ${NCD_ADDITIONAL_SOURCES}
)
target_link_libraries(badvpn-ncd system dhcpclient ncdconfig process)

install(
    TARGETS badvpn-ncd
    RUNTIME DESTINATION bin
)
