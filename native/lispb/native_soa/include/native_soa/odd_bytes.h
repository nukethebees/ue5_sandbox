#pragma once

#include <cstdint>

namespace ml::native_soa_fixture {
struct OddBytes {
    std::uint8_t bytes[3];
};

static_assert(sizeof(OddBytes) == 3);
}
