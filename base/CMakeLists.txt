set(BASE_ADDITIONAL_SOURCES)

if (NOT WIN32)
    list(APPEND BASE_ADDITIONAL_SOURCES BLog_syslog.c)
endif ()

add_library(base
    DebugObject.c
    BLog.c
    BPending.c
    ${BASE_ADDITIONAL_SOURCES}
)
