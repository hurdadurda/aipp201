## ----------------------------------------------------------------------
## Copyright 2026 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ----------------------------------------------------------------------

# Atlas is expected to be a pre-installed tool on the system. We do NOT
# fetch it; we just need to locate the executable so AtlasWithFormat.cmake
# can drive it from build rules.
message(STATUS "Locating Atlas executable...")
if (NOT DEFINED Atlas_EXECUTABLE)
    find_program(Atlas_EXECUTABLE
            NAMES atlas
            DOC "Atlas strong-type generator executable")
endif ()
if (NOT Atlas_EXECUTABLE)
    message(FATAL_ERROR
            "Atlas executable not found. Install atlas and ensure it is on PATH, "
            "or pass -DAtlas_EXECUTABLE=/absolute/path/to/atlas to cmake.")
endif ()
message(STATUS "Found Atlas: ${Atlas_EXECUTABLE}")

# Test dependencies (conditional)
if (AIPP_BUILD_TESTS)
    string(REGEX REPLACE "(^| )-g([0-9]?)( |$)" "\\1-g3\\3" tmp "${CMAKE_CXX_FLAGS_DEBUG}")
    if (NOT "${CMAKE_CXX_FLAGS_DEBUG}" STREQUAL "${tmp}")
        message(STATUS "Changing CMAKE_CXX_FLAGS_DEBUG from '${CMAKE_CXX_FLAGS_DEBUG}' to '${tmp}'")
        set(CMAKE_CXX_FLAGS_DEBUG "${tmp}")
    endif ()

    message(STATUS "Processing third-party DocTest...")
    FetchContent_Declare(
            DocTest
            GIT_REPOSITORY https://github.com/jodyhagins/doctest.git
            GIT_TAG dev
            SYSTEM
    )
    FetchContent_MakeAvailable(DocTest)

    message(STATUS "Processing third-party RapidCheck...")
    set(RC_ENABLE_DOCTEST ON)
    FetchContent_Declare(
            rapidcheck
            GIT_REPOSITORY https://github.com/jodyhagins/rapidcheck.git
            GIT_TAG wjh-master
            SYSTEM
    )
    FetchContent_MakeAvailable(rapidcheck)
endif ()
