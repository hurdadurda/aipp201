## ----------------------------------------------------------------------
## Copyright 2026 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ----------------------------------------------------------------------
cmake_minimum_required(VERSION 3.27)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" AIPP_VERSION_RAW)
string(STRIP "${AIPP_VERSION_RAW}" AIPP_VERSION)

project(aipp
        VERSION ${AIPP_VERSION}
        DESCRIPTION "AIPP C++ Starter"
        LANGUAGES CXX)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(Setup)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(AIPP_BUILD_TESTS "whether or not to build tests" ON)
    option(AIPP_SANITIZE "whether or not to use address sanitizer" ON)
endif ()

find_package(Threads REQUIRED)
include(FetchContent)
include(CTest)
include(CompileOptions)
if (AIPP_SANITIZE)
    add_compile_options(-fsanitize=address -g)
    add_link_options(-fsanitize=address)
endif ()
include(ThirdParty)
include(AtlasWithFormat)

create_aipp_compile_options_target()
link_libraries(aipp_compile_options)

add_subdirectory(src)
