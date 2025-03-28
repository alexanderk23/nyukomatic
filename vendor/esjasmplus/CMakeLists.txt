cmake_minimum_required(VERSION 3.18.0)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

project(esjasmplus)

set(SOURCES
    esjasm/devices.cpp
    esjasm/directives.cpp
    esjasm/lua_sjasm.cpp
    esjasm/parser.cpp
    esjasm/reader.cpp
    esjasm/relocate.cpp
    esjasm/support.cpp
    esjasm/tables.cpp
    esjasm/z80.cpp
    crc32c/crc32c.cpp
    esjasm/io_err.cpp
    esjasm/sjio.cpp
    esjasm/esjasm.cpp)

add_definitions(-DMAX_PATH=PATH_MAX)
add_definitions(-DCMAKE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -pedantic")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

include_directories(esjasm)
include_directories(crc32c)
add_library(esjasmplus STATIC ${SOURCES})
