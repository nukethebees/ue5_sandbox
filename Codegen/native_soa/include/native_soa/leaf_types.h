#pragma once

#include <cstdint>

namespace ml::native_experiment {

struct Handle {
    std::int32_t index{-1};
    std::int32_t generation{-1};
};

enum class Task : std::uint8_t { None };
enum class Team : std::uint8_t { None };

struct OddBytes {
    std::uint8_t bytes[3];
};

struct alignas(32) Aligned32 {
    std::int32_t value{32};
};

struct alignas(64) Aligned64 {
    std::int32_t value{64};
};

struct alignas(256) Aligned256 {
    std::int32_t value{256};
};

}
