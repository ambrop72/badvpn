add_library(security
    BEncryption.c
    BHash.c
    BRandom.c
    OTPCalculator.c
    OTPChecker.c
    OTPGenerator.c
)
target_link_libraries(security system ${LIBCRYPTO_LIBRARIES})
