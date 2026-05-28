// ----------------------------------------------------------------------
// Copyright 2026 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ----------------------------------------------------------------------
#include "testing/doctest.hpp"

// Anchor TU so the static library has at least one object file. The real
// content of this library lives in its headers (e.g. doctest.hpp, which
// configures doctest before including it).
namespace aipp::testing {

[[maybe_unused]] inline constexpr int testing_anchor = 0;

} // namespace aipp::testing
