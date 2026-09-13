#include "sandbox/simulation/ship_health.h"

#include <gtest/gtest.h>

TEST(ShipHealth, PreservesExistingClampBehaviour) {
    EXPECT_EQ(ml::simulation::clamp_health_to_max(50, 100), 100);
    EXPECT_EQ(ml::simulation::clamp_health_to_max(120, 100), 120);
}
