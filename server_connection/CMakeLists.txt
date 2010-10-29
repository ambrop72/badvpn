add_library(server_conection ServerConnection.c)
target_link_libraries(server_conection system flow nspr_support ${NSPR_LIBRARIES} ${NSS_LIBRARIES})
