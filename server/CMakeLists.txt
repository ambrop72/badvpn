add_executable(badvpn-server server.c)
target_link_libraries(badvpn-server system flow flowextra nspr_support predicate security ${NSPR_LIBRARIES} ${NSS_LIBRARIES})

install(
    TARGETS badvpn-server
    RUNTIME DESTINATION bin
)
