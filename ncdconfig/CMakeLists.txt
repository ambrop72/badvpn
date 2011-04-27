add_library(ncdconfig
    NCDConfig.c
    NCDConfigTokenizer.c
    NCDConfigParser.c
)
target_link_libraries(ncdconfig system)
