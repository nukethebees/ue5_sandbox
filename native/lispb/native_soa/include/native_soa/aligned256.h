#pragma once

#include <cstdint>

namespace ioj::native_soa_fixture {
struct alignas(256) Aligned256 {
    std::int32_t value{256};
};

static_assert(alignof(Aligned256) == 256);
}
