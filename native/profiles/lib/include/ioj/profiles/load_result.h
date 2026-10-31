#pragma once

#include <cstdint>

namespace ioj::profiles {
enum class LoadResult : std::uint8_t { succeeded, not_found, failed };
}
