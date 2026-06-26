cmake_minimum_required(VERSION 3.12)
project(libnu C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

include(CheckIncludeFile)
include(CheckFunctionExists)

check_include_file("sys/epoll.h" HAVE_SYS_EPOLL_H)
check_include_file("sys/inotify.h" HAVE_SYS_INOTIFY_H)

check_function_exists(timerfd_create HAVE_TIMERFD_CREATE)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

include_directories(include "${CMAKE_CURRENT_BINARY_DIR}")

set(LIB_SOURCES src/main.c)

# Build both Dynamic (.so) and Static (.a) libraries
add_library(nu_shared SHARED ${LIB_SOURCES})
add_library(nu_static STATIC ${LIB_SOURCES})

set_target_properties(nu_shared PROPERTIES OUTPUT_NAME nu)
set_target_properties(nu_static PROPERTIES OUTPUT_NAME nu)

# Position Independent Code is required for modern shared libraries
set_target_properties(nu_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(nu_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test1.c")
    add_executable(test_tool tests/test1.c)
    target_link_libraries(test_tool nu_shared)
endif()

include(GNUInstallDirs)

install(TARGETS nu_shared nu_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/libnu.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
