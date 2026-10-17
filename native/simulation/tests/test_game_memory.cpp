#include "support/simulation_test_support.h"
#include <ioj/sim/memory/game_memory.h>
#include <native/memory/retained_backing_resource.h>

namespace ioj::sim::tests {

TEST(NativeSimulation, GameMemoryTest) {
    GameMemory memory{{.root_capacity_bytes = 512}};

    std::byte* first_address{};
    {
        auto block{memory.acquire_block(128, 64)};
        first_address = block.data();
        EXPECT_EQ(memory.get_stats().live_block_bytes, std::size_t{128})
            << "A live block contributes its requested bytes";
    }
    auto reused{memory.acquire_block(128, 64)};
    EXPECT_EQ(reused.data(), first_address) << "A returned exact block is reused";

    auto exhaustion{memory.try_acquire_block(1024, 32)};
    EXPECT_FALSE(exhaustion.has_value()) << "Root exhaustion is nonfatal through the try API";
    auto const failure{memory.get_last_allocation_failure()};
    EXPECT_TRUE(failure.has_value()) << "Root exhaustion records a diagnostic";
    EXPECT_EQ(failure->requested_bytes, std::size_t{1024}) << "Diagnostic records requested bytes";
    EXPECT_EQ(failure->alignment, std::size_t{32}) << "Diagnostic records alignment";
    EXPECT_EQ(failure->claimed_bytes, std::size_t{128})
        << "Diagnostic records bytes already claimed";
    EXPECT_EQ(failure->total_capacity_bytes, std::size_t{512})
        << "Diagnostic records total capacity";

    memory::RetainedBackingResource upstream{1024};
    auto* const backing_data{upstream.data()};
    {
        GameMemory first{{.root_capacity_bytes = 256}, upstream};
        EXPECT_TRUE(upstream.is_in_use());
        EXPECT_FALSE(upstream.can_allocate(256, memory::Backing::alignment));
        EXPECT_EQ(first.backing_data(), backing_data);
    }
    EXPECT_FALSE(upstream.is_in_use());
    EXPECT_FALSE(upstream.can_allocate(2048, memory::Backing::alignment));
    EXPECT_FALSE(upstream.can_allocate(256, memory::Backing::alignment * 2));
    {
        GameMemory second{{.root_capacity_bytes = 512}, upstream};
        EXPECT_EQ(second.backing_data(), backing_data)
            << "Sequential game-memory instances reuse the retained allocation";
    }
    EXPECT_FALSE(upstream.is_in_use());
}

} // namespace tests
