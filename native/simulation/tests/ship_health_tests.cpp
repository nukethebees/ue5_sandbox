#include "ioj/sim/ship_health.h"

#include <gtest/gtest.h>

namespace ioj::sim::tests {

TEST(ShipHealth, PreservesExistingClampBehaviour) {
    EXPECT_EQ(clamp_health_to_max(50, 100), 100);
    EXPECT_EQ(clamp_health_to_max(120, 100), 120);
}

} // namespace tests
