add_executable(linkedlist2_example linkedlist2_example.c)

add_executable(btimer_example btimer_example.c)
target_link_libraries(btimer_example system)

add_executable(predicate_test predicate_test.c)
target_link_libraries(predicate_test predicate)

add_executable(fairqueue_test fairqueue_test.c)
target_link_libraries(fairqueue_test system flow)

add_executable(fairqueue_test2 fairqueue_test2.c)
target_link_libraries(fairqueue_test2 system flow security)

add_executable(bheap_test bheap_test.c)
target_link_libraries(bheap_test security)

add_executable(bavl_test bavl_test.c)
target_link_libraries(bavl_test security)

add_executable(bencryption_bench bencryption_bench.c)
target_link_libraries(bencryption_bench system security)

add_executable(ncd_tokenizer_test ncd_tokenizer_test.c)
target_link_libraries(ncd_tokenizer_test ncdconfig)

add_executable(ncd_parser_test ncd_parser_test.c)
target_link_libraries(ncd_parser_test ncdconfig)

if (NOT WIN32)
    add_executable(ipc_server ipc_server.c)
    target_link_libraries(ipc_server ipc)

    add_executable(ipc_client ipc_client.c)
    target_link_libraries(ipc_client ipc)

    add_executable(dhcpclient_test dhcpclient_test.c)
    target_link_libraries(dhcpclient_test dhcpclient)

    add_executable(bprocess_example bprocess_example.c)
    target_link_libraries(bprocess_example system process)
endif ()
