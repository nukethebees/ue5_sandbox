#pragma once

#include <cstdint>

namespace ioj::presentation {
struct EntityOverlayHealthMaximums {
    std::int32_t capital_ship{1};
    std::int32_t fighter{1};
    std::int32_t turret{1};
};
}
