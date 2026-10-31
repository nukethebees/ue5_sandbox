#include "ioj/sim/entities/ship_health.h"

#include <gtest/gtest.h>

namespace ioj::sim::tests {

TEST(ShipHealth, ClampsExcessHealthWithoutHealingDamage) {
    ShipHealth below_max{Health{50}, Health{100}};
    below_max.clamp_to_max();
    EXPECT_EQ(below_max.health.value(), 50);
    ShipHealth above_max{Health{120}, Health{100}};
    above_max.clamp_to_max();
    EXPECT_EQ(above_max.health.value(), 100);
}

} // namespace tests
