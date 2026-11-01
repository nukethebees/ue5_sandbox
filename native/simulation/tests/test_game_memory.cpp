#include "support/simulation_test_support.h"
#include <ioj/sim/memory/game_memory.h>
#include <native/memory/bump_resource.h>

namespace ioj::sim::tests {

TEST(NativeSimulation, GameMemoryTest) {
    memory::BumpResource backing{512};
    GameMemory memory{backing};
    {
        auto* const local{memory.allocate(128, 64)};
        auto* const overflow{memory.allocate(1024, 256)};
        auto const stats{memory.stats()};
        EXPECT_EQ(stats.live_bytes, 1152u);
        EXPECT_EQ(stats.live_allocation_count, 2u);
        EXPECT_EQ(stats.peak_live_bytes, 1152u);
        EXPECT_EQ(stats.peak_live_allocation_count, 2u);
        memory.deallocate(overflow, 1024, 256);
        memory.deallocate(local, 128, 64);
    }
    EXPECT_EQ(memory.stats().live_bytes, 0u);
    EXPECT_EQ(memory.stats().live_allocation_count, 0u);
    EXPECT_EQ(memory.stats().peak_live_bytes, 1152u);
    EXPECT_EQ(backing.used_bytes(), 128u);
    backing.reset();
    EXPECT_EQ(backing.used_bytes(), 0u);

    GameMemory failing{*std::pmr::null_memory_resource()};
    EXPECT_THROW((void)failing.allocate(32, 8), std::bad_alloc);
    EXPECT_EQ(failing.stats().live_allocation_count, 0u);
}
}
