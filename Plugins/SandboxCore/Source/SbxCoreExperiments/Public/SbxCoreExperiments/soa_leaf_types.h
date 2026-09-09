#pragma once

#include <CoreTypes.h>

namespace ml::single_allocation_experiment {

struct Handle {
    int32 index{-1};
    int32 generation{-1};
};

enum class Task : uint8 { None };
enum class Team : uint8 { None };

struct OddBytes {
    uint8 bytes[3];
};

struct alignas(32) Aligned32 {
    int32 value{32};
};

struct alignas(64) Aligned64 {
    int32 value{64};
};

struct alignas(256) Aligned256 {
    int32 value{256};
};

}
