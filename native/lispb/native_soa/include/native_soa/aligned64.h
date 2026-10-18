#pragma once

#include <cstdint>

namespace ioj::native_soa_fixture {
struct alignas(64) Aligned64 {
    std::int32_t value{64};
};

static_assert(alignof(Aligned64) == 64);
}
