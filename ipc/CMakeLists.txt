add_library(ipc
    BIPC.c
    BIPCServer.c
)
target_link_libraries(ipc system flow)
