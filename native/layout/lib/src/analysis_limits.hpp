#pragma once

#include <cstdint>
#include <limits>

namespace ioj::layout::analysis_detail {

// Counts and extents beyond this bound require an optional or ExactCodeCount result.
inline constexpr std::uint64_t maximum_analysis_count{std::numeric_limits<std::uint64_t>::max()};
inline constexpr std::uint64_t maximum_unsigned_code{std::numeric_limits<std::uint64_t>::max()};
inline constexpr std::int64_t minimum_signed_code{std::numeric_limits<std::int64_t>::min()};
inline constexpr std::int64_t maximum_signed_code{std::numeric_limits<std::int64_t>::max()};

// Bound exact periodic enumeration without treating a large period as arithmetic overflow.
inline constexpr std::uint64_t maximum_exact_region_period{1'000'000};

}
