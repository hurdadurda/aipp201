## ----------------------------------------------------------------------
## Copyright 2026 Jody Hagins
## Distributed under the MIT Software License
## See accompanying file LICENSE or copy at
## https://opensource.org/licenses/MIT
## ----------------------------------------------------------------------

add_executable(aipp_ut
        main.cpp
        Foo_ut.cpp
        SpscQueue_ut.cpp
)

target_link_libraries(aipp_ut
        PRIVATE
        aipp::aipp
        aipp::testing
        Threads::Threads
        rapidcheck_doctest
        doctest
)

add_test(NAME "AIPP Tests" COMMAND aipp_ut)
