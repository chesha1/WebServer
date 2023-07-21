cmake_minimum_required(VERSION 3.25)
project(WebServer CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(WebServer/include)

file(GLOB SRC WebServer/*.cpp)
file(GLOB HEADERS WebServer/include/*.h)

add_executable(WebServer ${SRC} ${HEADERS})
