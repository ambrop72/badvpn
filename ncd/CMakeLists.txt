include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(NCD_ADDITIONAL_SOURCES)

if (BADVPN_USE_LINUX_RFKILL)
    list(APPEND NCD_ADDITIONAL_SOURCES
        NCDRfkillMonitor.c
        modules/net_backend_rfkill.c
    )
endif ()

if (BADVPN_USE_LINUX_INPUT)
    list(APPEND NCD_ADDITIONAL_SOURCES
        modules/sys_evdev.c
    )
endif ()

if (BADVPN_USE_INOTIFY)
    list(APPEND NCD_ADDITIONAL_SOURCES
        modules/sys_watch_directory.c
    )
endif ()

add_library(ncdconfig
    NCDConfig.c
    NCDConfigTokenizer.c
    NCDConfigParser.c
)
target_link_libraries(ncdconfig base)

add_executable(badvpn-ncd
    ncd.c
    NCDValue.c
    NCDModule.c
    NCDModuleIndex.c
    NCDIfConfig.c
    NCDInterfaceMonitor.c
    BEventLock.c
    modules/command_template.c
    modules/event_template.c
    modules/var.c
    modules/list.c
    modules/depend.c
    modules/multidepend.c
    modules/concat.c
    modules/concatv.c
    modules/if.c
    modules/strcmp.c
    modules/regex_match.c
    modules/logical.c
    modules/sleep.c
    modules/print.c
    modules/blocker.c
    modules/ip_in_network.c
    modules/run.c
    modules/runonce.c
    modules/spawn.c
    modules/call.c
    modules/rimp_call.c
    modules/ref.c
    modules/index.c
    modules/alias.c
    modules/process_manager.c
    modules/ondemand.c
    modules/foreach.c
    modules/choose.c
    modules/net_backend_waitdevice.c
    modules/net_backend_waitlink.c
    modules/net_backend_badvpn.c
    modules/net_backend_wpa_supplicant.c
    modules/net_up.c
    modules/net_dns.c
    modules/net_iptables.c
    modules/net_ipv4_addr.c
    modules/net_ipv4_route.c
    modules/net_ipv4_dhcp.c
    modules/net_ipv4_arp_probe.c
    modules/net_watch_interfaces.c
    modules/sys_watch_input.c
    modules/sys_watch_usb.c
    ${NCD_ADDITIONAL_SOURCES}
)
target_link_libraries(badvpn-ncd system flow flowextra dhcpclient arpprobe ncdconfig udevmonitor)

if (BADVPN_USE_LINUX_INPUT)
    string(REPLACE " " ";" FLAGS_LIST "${CMAKE_C_FLAGS}")
    execute_process(COMMAND ${CMAKE_C_COMPILER} ${FLAGS_LIST} -E ${CMAKE_CURRENT_SOURCE_DIR}/include_linux_input.h
                    RESULT_VARIABLE LINUX_INPUT_PREPROCESS_RESULT
                    OUTPUT_VARIABLE LINUX_INPUT_PREPROCESS_OUTPUT)
    if (NOT LINUX_INPUT_PREPROCESS_RESULT EQUAL 0)
        message(FATAL_ERROR "failed to preprocess linux/input.h include")
    endif ()

    string(REGEX MATCH "\"(/[^\"]+/linux/input.h)\"" LINUX_INPUT_MATCH ${LINUX_INPUT_PREPROCESS_OUTPUT})
    if (NOT LINUX_INPUT_MATCH)
        message(FATAL_ERROR "failed to match preprocessor output for path of linux/input.h")
    endif ()
    set(LINUX_INPUT_H_PATH ${CMAKE_MATCH_1})

    message(STATUS "Generating linux_input_names.h from ${LINUX_INPUT_H_PATH}")

    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/parse_linux_input.sh 
                            ${LINUX_INPUT_H_PATH}
                            ${CMAKE_CURRENT_BINARY_DIR}/linux_input_names.h
                    RESULT_VARIABLE LINUX_INPUT_PARSE_RESULT)
    if (NOT LINUX_INPUT_PARSE_RESULT EQUAL 0)
        message(FATAL_ERROR "failed to generate linux_input_names.h")
    endif ()
endif ()

install(
    TARGETS badvpn-ncd
    RUNTIME DESTINATION bin
)
