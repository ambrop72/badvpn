add_library(nspr_support
    BSocketPRFileDesc.c
    DummyPRFileDesc.c
    BPRFileDesc.c
    PRStreamSource.c
    PRStreamSink.c
)
target_link_libraries(nspr_support system ${NSPR_LIBRARIES})
