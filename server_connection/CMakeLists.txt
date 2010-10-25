add_library(server_conection ServerConnection.c)
target_link_libraries(server_conection system flow nspr_support ${LIBCRYPTO_LIBRARIES} ${NSPR_LIBRARIES} ${NSS_LIBRARIES})
