## ----------------------------------------------------------------------
## Copyright 2026 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ----------------------------------------------------------------------
add_library(aipp_testing STATIC)
add_library(aipp::testing ALIAS aipp_testing)

target_sources(aipp_testing
        PRIVATE
        testing.cpp

        PUBLIC
        doctest.hpp
)

target_link_libraries(aipp_testing
        PUBLIC
        doctest
)

target_include_directories(aipp_testing
        PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
