// ----------------------------------------------------------------------
// Copyright 2026 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ----------------------------------------------------------------------
#include "Version.hpp"
#include "types_gen.hpp"

namespace aipp {

// Anchor TU so the static library has at least one object file.
// Project code lives in headers + generated types; replace this as
// real implementation is added.
[[maybe_unused]] inline constexpr auto library_version = version_string;

} // namespace aipp
