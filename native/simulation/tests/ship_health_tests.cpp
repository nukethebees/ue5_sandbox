#include "ioj/sim/entities/ship_health.h"

#include <gtest/gtest.h>

namespace ioj::sim::tests {

TEST(ShipHealth, PreservesExistingClampBehaviour) {
    ShipHealth below_max{50, 100};
    below_max.clamp_to_max();
    EXPECT_EQ(below_max.health, 100);
    ShipHealth above_max{120, 100};
    above_max.clamp_to_max();
    EXPECT_EQ(above_max.health, 120);
}

} // namespace tests
