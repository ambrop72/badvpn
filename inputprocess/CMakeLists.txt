add_library(inputprocess
    BInputProcess.c
)
target_link_libraries(inputprocess system flow process)
