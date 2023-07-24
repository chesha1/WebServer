#cmake_minimum_required(VERSION 3.25)
#project(WebServer CXX)
#
#set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_STANDARD_REQUIRED True)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#
#find_program(IWYU_PROGRAM NAMES include-what-you-use)
#if (IWYU_PROGRAM)
#    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PROGRAM})
#endif ()
#
#target_link_libraries(WebServer PRIVATE uring)
#
#include_directories(WebServer/include)
#
#
#file(GLOB SRC WebServer/*.cpp)
#file(GLOB HEADERS WebServer/include/*.h)
#
#add_executable(WebServer ${SRC} ${HEADERS})

cmake_minimum_required(VERSION 3.10)

project(WebServer)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(IWYU_PROGRAM NAMES include-what-you-use)
if(IWYU_PROGRAM)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PROGRAM})
endif()

file(GLOB SOURCE_FILE WebServer/*.cpp)
include_directories(WebServer/include)
add_executable(WebServer ${SOURCE_FILE})

target_include_directories(WebServer PRIVATE include)
target_compile_options(WebServer PRIVATE -Wall -Wextra)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_link_libraries(WebServer PRIVATE asan ubsan uring)
    target_compile_options(WebServer PRIVATE -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined)
else()
    target_link_libraries(WebServer PRIVATE uring)
endif()
