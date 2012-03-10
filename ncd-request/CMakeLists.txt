add_executable(badvpn-ncd-request
    ncd-request.c
)
target_link_libraries(badvpn-ncd-request ncdrequest)

install(
    TARGETS badvpn-ncd-request
    RUNTIME DESTINATION bin
)
