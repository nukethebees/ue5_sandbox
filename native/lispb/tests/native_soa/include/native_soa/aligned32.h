#pragma once

#include <cstdint>

namespace ioj::native_soa_fixture {
struct alignas(32) Aligned32 {
    std::int32_t value{32};
};

static_assert(alignof(Aligned32) == 32);
}
