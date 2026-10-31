#pragma once

#include <cstdint>

namespace ioj::profiles {
// UTC seconds since the Unix epoch.
using Timestamp = std::int64_t;

auto current_timestamp() -> Timestamp;
}
