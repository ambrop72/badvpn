cmake_minimum_required(VERSION 2.6)
project(BADVPN C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(TestBigEndian)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckTypeSize)

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
    lwip/custom
    lwip/src/include
    lwip/src/include/ipv4
)

link_directories(
    ${LIBCRYPTO_LIBRARY_DIRS}
    ${NSPR_LIBRARY_DIRS}
    ${NSS_LIBRARY_DIRS}
)

test_big_endian(BIG_ENDIAN)

check_type_size(int INT_SIZE)
if (NOT (INT_SIZE GREATER "3"))
    message(FATAL_ERROR "int must be at least 32 bits")
endif ()

add_definitions(-std=gnu99 -Werror=implicit-function-declaration -Wno-unused-value -Wno-parentheses -Wno-switch-enum -Wredundant-decls)

# platform-specific stuff
if (WIN32)
    add_definitions(-DBADVPN_USE_WINAPI -D_WIN32_WINNT=0x600 -DWIN32_LEAN_AND_MEAN)

    set(CMAKE_REQUIRED_DEFINITIONS "-D_WIN32_WINNT=0x600")
    check_symbol_exists(WSAID_WSARECVMSG "mswsock.h" HAVE_WSARECVMSG)
    check_symbol_exists(WSAID_WSASENDMSG "mswsock.h" HAVE_WSASENDMSG)
    set(CMAKE_REQUIRED_DEFINITIONS "")
    if (NOT (HAVE_WSARECVMSG AND HAVE_WSASENDMSG))
        add_definitions(-DBADVPN_USE_CUSTOM_MSWSOCK_H)
    endif ()
else ()
    set(BADVPN_THREADWORK_USE_PTHREAD 1)
    add_definitions(-DBADVPN_THREADWORK_USE_PTHREAD)

    link_libraries(rt)

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_definitions(-DBADVPN_LINUX)

        check_include_files(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
        if (NOT HAVE_SYS_SIGNALFD_H)
            message(FATAL_ERROR "signalfd is required")
        endif ()
        add_definitions(-DBADVPN_USE_SIGNALFD)

        check_include_files(sys/epoll.h HAVE_SYS_EPOLL_H)
        if (NOT HAVE_SYS_EPOLL_H)
            message(FATAL_ERROR "epoll is required")
        endif ()
        add_definitions(-DBADVPN_USE_EPOLL)

        check_include_files(linux/rfkill.h HAVE_LINUX_RFKILL_H)
        if (HAVE_LINUX_RFKILL_H)
            add_definitions(-DBADVPN_USE_LINUX_RFKILL)
            set(BADVPN_USE_LINUX_RFKILL 1)
        endif ()
    elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        add_definitions(-DBADVPN_FREEBSD)

        check_symbol_exists(kqueue "sys/types.h;sys/event.h;sys/time.h" HAVE_KQUEUE)
        if (NOT HAVE_KQUEUE)
            message(FATAL_ERROR "kqueue is required")
        endif ()
        add_definitions(-DBADVPN_USE_KEVENT)
    endif ()

    if (NOT DEFINED BADVPN_WITHOUT_CRYPTODEV)
        check_include_files(crypto/cryptodev.h HAVE_CRYPTO_CRYPTODEV_H)
        if (HAVE_CRYPTO_CRYPTODEV_H)
            add_definitions(-DBADVPN_USE_CRYPTODEV)
        elseif (DEFINED BADVPN_WITH_CRYPTODEV)
            message(FATAL_ERROR "crypto/cryptodev.h not found")
        endif ()
    endif ()
endif ()

# add preprocessor definitions
if (BIG_ENDIAN)
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
add_subdirectory(lwip)
add_subdirectory(dhcpclient)
add_subdirectory(ncdconfig)
add_subdirectory(threadwork)
if (NOT WIN32)
    add_subdirectory(ipc)
    add_subdirectory(process)
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

# tun2socks
add_subdirectory(tun2socks)

# ncd
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(ncd)
endif ()
