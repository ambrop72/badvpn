set(BASE_ADDITIONAL_SOURCES)

if (HAVE_SYSLOG_H)
    list(APPEND BASE_ADDITIONAL_SOURCES BLog_syslog.c)
endif ()

add_library(base
    DebugObject.c
    BLog.c
    BPending.c
    ${BASE_ADDITIONAL_SOURCES}
)
