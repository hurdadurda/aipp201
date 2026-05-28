// ----------------------------------------------------------------------
// Copyright 2026 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ----------------------------------------------------------------------
#include "testing/doctest.hpp"
#include "types_gen.hpp"

#include <rapidcheck.h>
#include <rapidcheck/doctest.h>

#include <cstdint>

namespace aipp::test {

TEST_CASE("Foo constructs and compares")
{
    Foo const a{std::int64_t{1}};
    Foo const b{std::int64_t{2}};
    Foo const c{std::int64_t{1}};

    CHECK(a == c);
    CHECK(a != b);
    CHECK(a < b);
    CHECK(b > a);
    CHECK(a <= c);
    CHECK(a >= c);
}

TEST_CASE("Foo retrieves underlying value")
{
    Foo const f{std::int64_t{42}};
    CHECK(atlas_value_for(f) == std::int64_t{42});
}

TEST_CASE("Foo spaceship total order")
{
    rc::doctest::check(
            "comparison agrees with underlying int64 ordering",
            [](std::int64_t x, std::int64_t y) {
                Foo const a{x};
                Foo const b{y};
                return ((a < b) == (x < y)) //
                        and ((a == b) == (x == y)) //
                        and ((a > b) == (x > y));
            });
}

} // namespace aipp::test
