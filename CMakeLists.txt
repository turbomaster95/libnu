cmake_minimum_required(VERSION 3.12)
project(libnu C)

option(NU_SHARED "Build shared Library." OFF)
option(NU_STATIC "Build static Library." ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused -O1 -g -Wno-unused-function -I${CMAKE_CURRENT_BINARY_DIR}/include -Wno-unused -DNDEBUG -Wl,--gc-sections -ffunction-sections -fdata-sections")

include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file("sys/epoll.h" HAVE_SYS_EPOLL_H)
check_include_file("sys/inotify.h" HAVE_SYS_INOTIFY_H)
check_include_file("sys/signalfd.h" HAVE_SYS_SIGNALFD_H)
check_function_exists(timerfd_create HAVE_TIMERFD_CREATE)
check_function_exists(signalfd HAVE_SIGNALFD)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
)

include_directories(include "${CMAKE_CURRENT_BINARY_DIR}")

set(LIB_SOURCES
	src/ipc.c
	src/loop.c
	src/str.c
	src/proc.c
	src/log.c
	src/sig.c
	src/mm.c
	src/map.c
	src/lex.c
	src/ast.c
	src/pratt.c
	src/xtea.c
	src/hash.c
	src/hashes/md5.c
	src/hashes/sha1.c
	src/hashes/sha256.c
	src/json.c
	src/trie.c
	src/lz4.c
	src/time.c
	src/print.c
	src/argpars.c
	src/lua.c
	src/py.c
	src/third-party/pocketpy.c
	src/buf.c
	src/strsp.c
	src/std/string.c
	src/std/stdlib.c
	src/calc.c
)

set(NU_TARGETS "")

if(NU_STATIC)
    add_library(nu_static STATIC ${LIB_SOURCES})
    set_target_properties(nu_static PROPERTIES OUTPUT_NAME nu)
    set_target_properties(nu_static PROPERTIES 
		POSITION_INDEPENDENT_CODE ON
    )
    list(APPEND NU_TARGETS nu_static)
endif()

if(NU_SHARED)
    add_library(nu_shared SHARED ${LIB_SOURCES})
    set_target_properties(nu_shared PROPERTIES OUTPUT_NAME nu)
    set_target_properties(nu_shared PROPERTIES 
		POSITION_INDEPENDENT_CODE ON
    )
    list(APPEND NU_TARGETS nu_shared)
endif()

if(NU_STATIC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test1.c")
    add_executable(test_tool tests/test1.c)
    target_link_libraries(test_tool nu_static)
endif()

include(GNUInstallDirs)

install(TARGETS ${NU_TARGETS}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/nu.h  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/nuo.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/nus.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

