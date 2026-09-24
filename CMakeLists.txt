cmake_minimum_required(VERSION 3.12)
project(libnu C)

option(NU_SHARED "Build shared Library." OFF)
option(NU_STATIC "Build static Library." ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused -O1 -g -Wno-unused-function -I${CMAKE_CURRENT_BINARY_DIR}/include -Wno-unused -DNDEBUG -Wl,--gc-sections -ffunction-sections -fdata-sections -fno-builtin -D_GNU_SOURCE")

include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file("sys/epoll.h" HAVE_SYS_EPOLL_H)
check_include_file("sys/inotify.h" HAVE_SYS_INOTIFY_H)
check_include_file("sys/signalfd.h" HAVE_SYS_SIGNALFD_H)
check_include_file("ucontext.h" HAVE_UCONTEXT_H)
check_include_file("sys/wait.h" HAVE_SYS_WAIT_H)
check_function_exists(timerfd_create HAVE_TIMERFD_CREATE)
check_function_exists(signalfd HAVE_SIGNALFD)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
)

include_directories(
    include
    "${CMAKE_CURRENT_BINARY_DIR}"
)

set(LIB_SOURCES
    src/core/main.c
    src/core/log.c

    src/memory/mm.c
    src/memory/mm/arena.c
    src/memory/mm/buddy.c
    src/memory/mm/slab.c
    src/memory/mm/slob.c

    src/containers/buf.c
    src/containers/hash.c
    src/containers/map.c
    src/containers/trie.c

    src/strings/str.c
    src/strings/strsp.c

    src/std/string.c
    src/std/stdlib.c

    src/io/print.c

    src/time/time.c

    src/math/calc.c

    src/parsing/lex.c
    src/parsing/ast.c
    src/parsing/pratt.c

    src/crypto/xtea.c
    src/crypto/hashes/md5.c
    src/crypto/hashes/sha1.c
    src/crypto/hashes/sha256.c

    src/compression/lz4.c

    src/serialz/json.c

    src/async/loop.c

    src/process/ipc.c
    src/process/proc.c
    src/process/sig.c

    src/cli/argpars.c

    src/scripting/lua.c
    src/scripting/py.c
    src/scripting/third-party/pocketpy.c
)

if (CMAKE_SYSTEM_NAME STREQUAL "Generic" OR WASI)
    list(REMOVE_ITEM LIB_SOURCES "src/process/ipc.c")
    list(REMOVE_ITEM LIB_SOURCES "src/async/loop.c")
    list(REMOVE_ITEM LIB_SOURCES "src/process/proc.c")
    list(REMOVE_ITEM LIB_SOURCES "src/process/sig.c")
    list(REMOVE_ITEM LIB_SOURCES "src/scripting/lua.c")
endif()

set(NU_TARGETS "")

if(NU_STATIC)
    add_library(nu_static STATIC ${LIB_SOURCES})

    set_target_properties(nu_static PROPERTIES
        OUTPUT_NAME nu
        POSITION_INDEPENDENT_CODE ON
    )

    list(APPEND NU_TARGETS nu_static)
endif()

if(NU_SHARED)
    add_library(nu_shared SHARED ${LIB_SOURCES})

    set_target_properties(nu_shared PROPERTIES
        OUTPUT_NAME nu
        POSITION_INDEPENDENT_CODE ON
    )

    list(APPEND NU_TARGETS nu_shared)
endif()

include(GNUInstallDirs)

install(TARGETS ${NU_TARGETS}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    include/nu.h
    include/nuo.h
    include/nus.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
