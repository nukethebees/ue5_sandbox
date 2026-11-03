#include "support/simulation_test_support.h"
#include <ioj/memory/bump_resource.h>
#include <ioj/sim/levels/level_memory_manager.h>
#include <ioj/sim/memory/game_memory.h>

#include <array>
#include <thread>

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

    {
        LevelMemoryManager manager{memory, 64, 64};
        EXPECT_EQ(manager.level_memory().capacity_bytes(), 1024 * 1024);
        auto& tick{manager.tick_memory()};
        auto& phase{manager.phase_memory()};
        auto* const retained{static_cast<std::byte*>(tick.allocate(32, 1))};
        retained[0] = std::byte{42};

        for (int phase_index{}; phase_index < 2; ++phase_index) {
            FrameScratchScope scope{phase};
            auto* const temporary{phase.allocate(32, 1)};
            phase.deallocate(temporary, 32, 1);
        }
        EXPECT_EQ(phase.stats().current_claimed_bytes, 0u);
        EXPECT_EQ(tick.stats().current_claimed_bytes, 32u);
        EXPECT_EQ(retained[0], std::byte{42});

        std::array<std::jthread, 4> workers;
        auto const worker_count{workers.size()};
        for (std::size_t index{}; index < worker_count; ++index) {
            workers[index] = std::jthread{[&, index] {
                auto& resource{index % 2 == 0 ? tick : phase};
                for (int iteration{}; iteration < 64; ++iteration) {
                    auto* const allocation{resource.allocate(128, 16)};
                    resource.deallocate(allocation, 128, 16);
                }
            }};
        }
        for (auto& worker : workers) {
            worker.join();
        }
        EXPECT_EQ(manager.game_memory().stats().live_allocation_count, 3u);
        EXPECT_EQ(tick.stats().overflow_count, 128u);
        EXPECT_EQ(phase.stats().overflow_count, 128u);

        tick.deallocate(retained, 32, 1);
        tick.reset();
        phase.reset();
        EXPECT_EQ(tick.stats().current_claimed_bytes, 0u);
    }
    EXPECT_EQ(memory.stats().live_allocation_count, 0u);

    GameMemory failing{*std::pmr::null_memory_resource()};
    EXPECT_THROW((void)failing.allocate(32, 8), std::bad_alloc);
    EXPECT_EQ(failing.stats().live_allocation_count, 0u);
}
}
