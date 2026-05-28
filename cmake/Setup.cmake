## ----------------------------------------------------------------------
## Copyright 2026 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ----------------------------------------------------------------------
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD "20")
endif ()

option(AIPP_DETERMINE_CACHE_LINE_SIZE
        "Compute value for AIPP_CACHE_LINE_SIZE if not set"
        ON)

if (NOT AIPP_CACHE_LINE_SIZE AND AIPP_DETERMINE_CACHE_LINE_SIZE)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin" OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        execute_process(
                COMMAND ${CMAKE_SOURCE_DIR}/scripts/hw-cache-line-size ${CMAKE_SYSTEM_NAME}
                OUTPUT_VARIABLE AIPP_CACHE_LINE_SIZE
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else ()
        message(FATAL_ERROR "Unrecognized CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
    endif ()
    message(STATUS "Setting AIPP_CACHE_LINE_SIZE to computed value of "
            ${AIPP_CACHE_LINE_SIZE}
    )
endif ()
if (NOT AIPP_CACHE_LINE_SIZE)
    message(FATAL_ERROR "AIPP_CACHE_LINE_SIZE is not set.")
endif ()
