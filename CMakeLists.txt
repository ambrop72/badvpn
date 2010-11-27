cmake_minimum_required(VERSION 2.6)
project(BADVPN C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(TestBigEndian)
include(CheckIncludeFiles)

find_package(OpenSSL REQUIRED)
set(LIBCRYPTO_INCLUDE_DIRS "${OpenSSL_INCLUDE_DIRS}")
set(LIBCRYPTO_LIBRARY_DIRS "${OpenSSL_LIBRARY_DIRS}")
set(LIBCRYPTO_LIBRARIES "${OpenSSL_LIBRARIES}")

find_package(NSPR REQUIRED)
find_package(NSS REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCRYPTO_INCLUDE_DIRS}
    ${NSPR_INCLUDE_DIRS}
    ${NSS_INCLUDE_DIRS}
)

link_directories(
    ${LIBCRYPTO_LIBRARY_DIRS}
    ${NSPR_LIBRARY_DIRS}
    ${NSS_LIBRARY_DIRS}
)

test_big_endian(BIG_ENDIAN)

add_definitions(-std=gnu99 -Werror=implicit-function-declaration -Wno-unused-value -Wno-parentheses -Wno-switch-enum)

# platform-specific stuff
if (WIN32)
    add_definitions(-DBADVPN_USE_WINAPI -D_WIN32_WINNT=0x501 -DWIN32_LEAN_AND_MEAN)
else ()
    link_libraries(rt)

    check_include_files(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
    if (NOT ${HAVE_SYS_SIGNALFD_H})
        message(FATAL_ERROR "signalfd is required")
    endif ()

    check_include_files(sys/epoll.h HAVE_SYS_EPOLL_H)
    if (NOT ${HAVE_SYS_EPOLL_H})
        message(FATAL_ERROR "epoll is required")
    endif ()

    if (NOT DEFINED BADVPN_WITHOUT_CRYPTODEV)
        check_include_files(crypto/cryptodev.h HAVE_CRYPTO_CRYPTODEV_H)
        if (${HAVE_CRYPTO_CRYPTODEV_H})
            add_definitions(-DBADVPN_USE_CRYPTODEV)
        elseif (DEFINED BADVPN_WITH_CRYPTODEV)
            message(FATAL_ERROR "crypto/cryptodev.h not found")
        endif ()
    endif ()
endif ()

# add preprocessor definitions
if (${BIG_ENDIAN})
    add_definitions(-DBADVPN_BIG_ENDIAN)
else ()
    add_definitions(-DBADVPN_LITTLE_ENDIAN)
endif ()

# install man pages
install(
    FILES badvpn.7
    DESTINATION share/man/man7
)
install(
    FILES badvpn-server.8 badvpn-client.8
    DESTINATION share/man/man8
)

# internal libraries
add_subdirectory(system)
add_subdirectory(flow)
add_subdirectory(tuntap)
add_subdirectory(predicate)
add_subdirectory(nspr_support)
add_subdirectory(server_connection)
add_subdirectory(security)
add_subdirectory(socksclient)
if (NOT WIN32)
    add_subdirectory(ipc)
endif ()

# example programs
add_subdirectory(examples)

# tests
add_subdirectory(tests)

# server
add_subdirectory(server)

# client
add_subdirectory(client)

# flooder
add_subdirectory(flooder)
