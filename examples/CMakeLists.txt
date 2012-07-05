add_executable(linkedlist2_example linkedlist2_example.c)

add_executable(btimer_example btimer_example.c)
target_link_libraries(btimer_example system)

if (BUILDING_PREDICATE)
    add_executable(predicate_test predicate_test.c)
    target_link_libraries(predicate_test predicate)
endif ()

add_executable(fairqueue_test fairqueue_test.c)
target_link_libraries(fairqueue_test system flow)

add_executable(indexedlist_test indexedlist_test.c)

if (BUILDING_SECURITY)
    add_executable(fairqueue_test2 fairqueue_test2.c)
    target_link_libraries(fairqueue_test2 system flow security)

    add_executable(bheap_test bheap_test.c)
    target_link_libraries(bheap_test security)

    add_executable(bavl_test bavl_test.c)
    target_link_libraries(bavl_test security)

    add_executable(bencryption_bench bencryption_bench.c)
    target_link_libraries(bencryption_bench system security)
endif ()

if (BUILD_NCD)
    add_executable(ncd_tokenizer_test ncd_tokenizer_test.c)
    target_link_libraries(ncd_tokenizer_test ncdtokenizer)

    add_executable(ncd_parser_test ncd_parser_test.c)
    target_link_libraries(ncd_parser_test ncdconfigparser ncdvaluegenerator)

    add_executable(ncd_value_parser_test ncd_value_parser_test.c)
    target_link_libraries(ncd_value_parser_test ncdvalueparser ncdvaluegenerator)

    add_executable(ncdinterfacemonitor_test ncdinterfacemonitor_test.c)
    target_link_libraries(ncdinterfacemonitor_test ncdinterfacemonitor)

    add_executable(ncdval_test ncdval_test.c)
    target_link_libraries(ncdval_test ncdval)
endif ()

if (BUILDING_UDEVMONITOR)
    add_executable(ncdudevmonitor_test ncdudevmonitor_test.c)
    target_link_libraries(ncdudevmonitor_test udevmonitor)

    add_executable(ncdudevmanager_test ncdudevmanager_test.c)
    target_link_libraries(ncdudevmanager_test udevmonitor)
endif ()

if (NOT WIN32)
    add_executable(bprocess_example bprocess_example.c)
    target_link_libraries(bprocess_example system)

    add_executable(stdin_input stdin_input.c)
    target_link_libraries(stdin_input system flow flowextra)
endif ()

if (BUILDING_DHCPCLIENT)
    add_executable(dhcpclient_test dhcpclient_test.c)
    target_link_libraries(dhcpclient_test dhcpclient)
endif ()

if (BUILDING_ARPPROBE)
    add_executable(arpprobe_test arpprobe_test.c)
    target_link_libraries(arpprobe_test arpprobe)
endif ()

add_executable(substring_test substring_test.c)
