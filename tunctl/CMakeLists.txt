add_executable(badvpn-tunctl tunctl.c)

install(
    TARGETS badvpn-tunctl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
