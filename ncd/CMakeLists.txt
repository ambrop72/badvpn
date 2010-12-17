add_executable(badvpn-ncd
    ncd.c
    NCDIfConfig.c
    NCDInterfaceMonitor.c
    interface_modules/interface_physical.c
)
target_link_libraries(badvpn-ncd system dhcpclient ncdconfig)

install(
    TARGETS badvpn-ncd
    RUNTIME DESTINATION bin
)
