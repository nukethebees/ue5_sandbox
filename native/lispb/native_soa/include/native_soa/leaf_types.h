#pragma once

#include <cstdint>

namespace ml::native_soa_fixture {

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

static_assert(sizeof(OddBytes) == 3);
static_assert(alignof(Aligned32) == 32);
static_assert(alignof(Aligned64) == 64);
static_assert(alignof(Aligned256) == 256);

}
