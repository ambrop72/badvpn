cmake_minimum_required(VERSION 2.8)
project(BADVPN C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(TestBigEndian)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckTypeSize)

set(BUILD_COMPONENTS)

macro (build_switch name default)
    if (NOT DEFINED BUILD_${name})
        if (BUILD_NOTHING_BY_DEFAULT)
            set(BUILD_${name} 0)
        else ()
            set(BUILD_${name} "${default}")
        endif ()
    endif ()
    list(APPEND BUILD_COMPONENTS "${name}")
endmacro ()

# define build defaults
build_switch(EXAMPLES 1)
build_switch(TESTS 1)
build_switch(SERVER 1)
build_switch(CLIENT 1)
build_switch(FLOODER 1)
build_switch(TUN2SOCKS 1)
build_switch(UDPGW 1)
build_switch(NCD 1)

if (BUILD_NCD AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR, "NCD is only available on Linux")
endif ()

if (BUILD_CLIENT OR BUILD_SERVER OR BUILD_NCD)
    find_package(OpenSSL REQUIRED)
    set(LIBCRYPTO_INCLUDE_DIRS "${OpenSSL_INCLUDE_DIRS}")
    set(LIBCRYPTO_LIBRARY_DIRS "${OpenSSL_LIBRARY_DIRS}")
    set(LIBCRYPTO_LIBRARIES "${OpenSSL_LIBRARIES}")
endif ()

if (BUILD_SERVER OR BUILD_CLIENT OR BUILD_FLOODER)
    find_package(NSPR REQUIRED)
    find_package(NSS REQUIRED)
endif ()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCRYPTO_INCLUDE_DIRS}
    ${NSPR_INCLUDE_DIRS}
    ${NSS_INCLUDE_DIRS}
    ${FUSE_INCLUDE_DIRS}
    lwip/custom
    lwip/src/include
    lwip/src/include/ipv4
)

link_directories(
    ${LIBCRYPTO_LIBRARY_DIRS}
    ${NSPR_LIBRARY_DIRS}
    ${NSS_LIBRARY_DIRS}
    ${FUSE_LIBRARY_DIRS}
)

add_definitions(
    ${FUSE_CFLAGS}
)

test_big_endian(BIG_ENDIAN)

check_type_size(int INT_SIZE)
if (NOT (INT_SIZE GREATER "3"))
    message(FATAL_ERROR "int must be at least 32 bits")
endif ()

check_type_size(size_t SIZE_SIZE)
if (NOT (SIZE_SIZE GREATER INT_SIZE OR SIZE_SIZE EQUAL INT_SIZE))
    message(FATAL_ERROR "size_t must be greater or equal than int")
endif ()

add_definitions(-std=gnu99 -Werror=implicit-function-declaration -Wno-unused-value -Wno-parentheses -Wno-switch-enum -Wredundant-decls)

# platform-specific stuff
if (WIN32)
    add_definitions(-DBADVPN_USE_WINAPI -D_WIN32_WINNT=0x600 -DWIN32_LEAN_AND_MEAN)

    set(CMAKE_REQUIRED_DEFINITIONS "-D_WIN32_WINNT=0x600")
    check_symbol_exists(WSAID_WSASENDMSG "mswsock.h" HAVE_MSW_1)
    check_symbol_exists(WSAID_WSARECVMSG "mswsock.h" HAVE_MSW_2)
    check_symbol_exists(WSAID_ACCEPTEX "mswsock.h" HAVE_MSW_3)
    check_symbol_exists(WSAID_GETACCEPTEXSOCKADDRS "mswsock.h" HAVE_MSW_4)
    check_symbol_exists(WSAID_CONNECTEX "mswsock.h" HAVE_MSW_5)
    set(CMAKE_REQUIRED_DEFINITIONS "")
    if (NOT (HAVE_MSW_1 AND HAVE_MSW_2 AND HAVE_MSW_3 AND HAVE_MSW_4 AND HAVE_MSW_5))
        add_definitions(-DBADVPN_USE_SHIPPED_MSWSOCK)
        check_type_size(WSAMSG HAVE_WSAMSG)
        if (NOT HAVE_WSAMSG)
            add_definitions(-DBADVPN_SHIPPED_MSWSOCK_DECLARE_WSAMSG)
        endif ()
    endif ()
else ()
    set(BADVPN_THREADWORK_USE_PTHREAD 1)
    add_definitions(-DBADVPN_THREADWORK_USE_PTHREAD)

    link_libraries(rt)

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_definitions(-DBADVPN_LINUX)

        check_include_files(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
        if (HAVE_SYS_SIGNALFD_H)
            add_definitions(-DBADVPN_USE_SIGNALFD)
        else ()
            add_definitions(-DBADVPN_USE_SELFPIPE)
        endif ()

        check_include_files(sys/epoll.h HAVE_SYS_EPOLL_H)
        if (HAVE_SYS_EPOLL_H)
            add_definitions(-DBADVPN_USE_EPOLL)
        else ()
            add_definitions(-DBADVPN_USE_POLL)
        endif ()

        check_include_files(linux/rfkill.h HAVE_LINUX_RFKILL_H)
        if (HAVE_LINUX_RFKILL_H)
            add_definitions(-DBADVPN_USE_LINUX_RFKILL)
            set(BADVPN_USE_LINUX_RFKILL 1)
        endif ()

        check_include_files(linux/input.h HAVE_LINUX_INPUT_H)
        if (HAVE_LINUX_INPUT_H)
            add_definitions(-DBADVPN_USE_LINUX_INPUT)
            set(BADVPN_USE_LINUX_INPUT 1)
        endif ()

        check_include_files(sys/inotify.h HAVE_SYS_INOTIFY_H)
        if (HAVE_SYS_INOTIFY_H)
            add_definitions(-DBADVPN_USE_INOTIFY)
            set(BADVPN_USE_INOTIFY 1)
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

# reset variables indicating whether we're building various libraries,
# and set them in the respective CMakeLists files. This is used to disable
# building examples and tests which require libraries that are not available.
set(BUILDING_SECURITY 0)
set(BUILDING_DHCPCLIENT 0)
set(BUILDING_BKIO 0)
set(BUILDING_PREDICATE 0)
set(BUILDING_UDEVMONITOR 0)
set(BUILDING_THREADWORK 0)

# internal libraries
add_subdirectory(base)
add_subdirectory(system)
add_subdirectory(flow)
add_subdirectory(flowextra)
if (OpenSSL_FOUND)
    set(BUILDING_SECURITY 1)
    add_subdirectory(security)
endif ()
if (NSS_FOUND)
    add_subdirectory(nspr_support)
endif ()
if (BUILD_CLIENT OR BUILDING_SECURITY)
    set(BUILDING_THREADWORK 1)
    add_subdirectory(threadwork)
endif ()
if (BUILD_CLIENT OR BUILD_TUN2SOCKS)
    add_subdirectory(tuntap)
endif ()
if (BUILD_SERVER)
    set(BUILDING_PREDICATE 1)
    add_subdirectory(predicate)
endif ()
if (BUILD_CLIENT OR BUILD_FLOODER)
    add_subdirectory(server_connection)
endif ()
if (BUILD_NCD)
    set(BUILDING_DHCPCLIENT 1)
    set(BUILDING_UDEVMONITOR 1)
    add_subdirectory(stringmap)
    add_subdirectory(udevmonitor)
    add_subdirectory(dhcpclient)
endif ()
if (BUILD_TUN2SOCKS)
    add_subdirectory(socksclient)
    add_subdirectory(udpgw_client)
    add_subdirectory(lwip)
endif ()

# example programs
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

# tests
if (BUILD_TESTS)
    add_subdirectory(tests)
endif ()

# server
if (BUILD_SERVER)
    add_subdirectory(server)
endif ()

# client
if (BUILD_CLIENT)
    add_subdirectory(client)
endif ()

# flooder
if (BUILD_FLOODER)
    add_subdirectory(flooder)
endif ()

# tun2socks
if (BUILD_TUN2SOCKS)
    add_subdirectory(tun2socks)
endif ()

# udpgw
if (BUILD_UDPGW)
    add_subdirectory(udpgw)
endif ()

# ncd
if (BUILD_NCD)
    add_subdirectory(ncd)
endif ()

message(STATUS "Building components:")

# print what we're building and what not
foreach (name ${BUILD_COMPONENTS})
    # to lower name
    string(TOLOWER "${name}" name_withspaces)

    # append spaces to name
    #while (TRUE)
    #    string(LENGTH "${name_withspaces}" length)
    #    if (NOT (length LESS 12))
    #        break()
    #    endif ()
    #    set(name_withspaces "${name_withspaces} ")
    #endwhile ()
    
    # determine if we're building
    if (BUILD_${name})
        set(building "yes")
    else ()
        set(building "no")
    endif ()
    
    message(STATUS "    ${name_withspaces} ${building}")
endforeach ()
