set(BSYSTEM_ADDITIONAL_LIBS)
if (WIN32)
    list(APPEND BSYSTEM_ADDITIONAL_LIBS ws2_32)
endif ()

set(BSYSTEM_ADDITIONAL_SOURCES)
if (NOT WIN32)
    list(APPEND BSYSTEM_ADDITIONAL_SOURCES
        BLog_syslog.c
        BUnixSignal.c
        BProcess.c
    )
endif ()

add_library(system
    BReactor.c
    BSignal.c
    BSocket.c
    BLog.c
    BTime.c
    DebugObject.c
    BPending.c
    Listener.c
    ${BSYSTEM_ADDITIONAL_SOURCES}
)
target_link_libraries(system ${BSYSTEM_ADDITIONAL_LIBS})
