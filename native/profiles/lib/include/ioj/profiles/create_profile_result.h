#pragma once

#include <cstdint>

namespace ioj::profiles {
enum class CreateProfileResult : std::uint8_t {
    succeeded,
    empty_name,
    name_too_long,
    duplicate_name,
    persistence_failed
};
}
