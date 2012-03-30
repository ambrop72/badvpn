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

if (BREACTOR_BACKEND STREQUAL "badvpn")
    list(APPEND BSYSTEM_ADDITIONAL_SOURCES BReactor_badvpn.c)
elseif (BREACTOR_BACKEND STREQUAL "glib")
    list(APPEND BSYSTEM_ADDITIONAL_SOURCES BReactor_glib.c)
    list(APPEND BSYSTEM_ADDITIONAL_LIBS ${GLIB2_LIBRARIES})
endif ()

add_library(system
    BSignal.c
    BTime.c
    BNetwork.c
    ${BSYSTEM_ADDITIONAL_SOURCES}
)
target_link_libraries(system base flow ${BSYSTEM_ADDITIONAL_LIBS})
