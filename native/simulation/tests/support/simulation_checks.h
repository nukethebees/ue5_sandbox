#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/testing/entity_observations.h>

#include <gtest/gtest.h>

namespace ioj::sim::tests {
inline void expect_frame_reclaimed(FrameMemoryStats const& stats) {
    EXPECT_EQ(stats.outstanding_allocation_count, 0u);
    EXPECT_EQ(stats.current_claimed_bytes, 0u);
}

inline void expect_entity_unregistered(LevelSim const& simulation, EntityId const id) {
    SCOPED_TRACE(::testing::PrintToString(id));
    EXPECT_EQ(observe_entity_row(simulation, id), EntityInstanceHandle::invalid_value);
    EXPECT_FALSE(observe_entity(simulation, id));
    EXPECT_FALSE(observe_live_entity(simulation, id));
}

}
