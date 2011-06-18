set(BSYSTEM_ADDITIONAL_LIBS)
set(BSYSTEM_ADDITIONAL_SOURCES)

if (WIN32)
    list(APPEND BSYSTEM_ADDITIONAL_LIBS ws2_32 mswsock)
    list(APPEND BSYSTEM_ADDITIONAL_SOURCES
        BConnection_win.c
        BDatagram_win.c
    )
endif ()

if (NOT WIN32)
    list(APPEND BSYSTEM_ADDITIONAL_SOURCES
        BUnixSignal.c
        BConnection_unix.c
        BDatagram_unix.c
        BProcess.c
        BInputProcess.c
    )
endif ()

add_library(system
    BReactor.c
    BSignal.c
    BTime.c
    BNetwork.c
    ${BSYSTEM_ADDITIONAL_SOURCES}
)
target_link_libraries(system base flow ${BSYSTEM_ADDITIONAL_LIBS})
