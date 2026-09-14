#include "ioj/sim/ship_health.h"

#include <gtest/gtest.h>

namespace ioj::sim::tests {

TEST(ShipHealth, PreservesExistingClampBehaviour) {
    EXPECT_EQ(ioj::sim::clamp_health_to_max(50, 100), 100);
    EXPECT_EQ(ioj::sim::clamp_health_to_max(120, 100), 120);
}

} // namespace ioj::sim::tests
