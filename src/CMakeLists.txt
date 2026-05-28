## ----------------------------------------------------------------------
## Copyright 2026 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ----------------------------------------------------------------------

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Version.hpp.in
        ${CMAKE_CURRENT_BINARY_DIR}/Version.hpp
        @ONLY)

add_library(aipp STATIC)
add_library(aipp::aipp ALIAS aipp)

atlas_strong_types_from_file(
        INPUT ${CMAKE_CURRENT_SOURCE_DIR}/types.atlas
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/types_gen.hpp
        TARGET aipp
)

target_sources(aipp
        PRIVATE
        aipp.cpp

        PUBLIC
        types_gen.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/Version.hpp
        SpscQueue.hpp
)

target_link_libraries(aipp
        PUBLIC
        Threads::Threads
)

target_include_directories(aipp
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

if (AIPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(testing)
    add_subdirectory(tests)
endif ()
