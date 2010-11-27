set(TUNTAP_ADDITIONAL_SOURCES)
if (WIN32)
    list(APPEND TUNTAP_ADDITIONAL_SOURCES tapwin32-funcs.c)
endif ()

add_library(tuntap
    BTap.c
    ${TUNTAP_ADDITIONAL_SOURCES}
)
target_link_libraries(tuntap system flow)
