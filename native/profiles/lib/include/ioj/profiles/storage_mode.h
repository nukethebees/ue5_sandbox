#pragma once

#include <cstdint>

namespace ioj::profiles {
enum class StorageMode : std::uint8_t { platform, shared, local, experimental };
}
