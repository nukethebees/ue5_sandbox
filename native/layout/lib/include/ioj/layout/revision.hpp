#pragma once

#include <cstdint>
#include <limits>

namespace ioj::layout {

using Revision = std::uint64_t;
inline constexpr Revision invalid_revision{std::numeric_limits<Revision>::max()};

}
