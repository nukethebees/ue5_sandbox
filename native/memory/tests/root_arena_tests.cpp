#include "native/memory/root_arena.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <utility>

namespace ioj::memory::tests {
class RootArenaTest : public testing::Test {
  protected:
    alignas(64) std::array<std::byte, 512> backing_{};
    RootArena arena_{backing_.data(), backing_.size()};
};

TEST_F(RootArenaTest, ReusesReturnedBlocks) {
    std::byte* first_address{};
    {
        auto block{arena_.try_acquire_block(128, 64)};
        ASSERT_TRUE(block);
        first_address = block.data();
        EXPECT_EQ(arena_.get_statistics().live_block_bytes, 128);
    }

    auto reused{arena_.try_acquire_block(128, 64)};
    ASSERT_TRUE(reused);
    EXPECT_EQ(reused.data(), first_address);
    EXPECT_EQ(arena_.get_statistics().claimed_bytes, 128);
}

TEST_F(RootArenaTest, ReportsExactExhaustionDiagnostics) {
    auto live_block{arena_.try_acquire_block(128, 64)};
    ASSERT_TRUE(live_block);
    EXPECT_FALSE(arena_.try_acquire_block(1024, 32));

    auto const failure{arena_.get_last_allocation_failure()};
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->requested_bytes, 1024);
    EXPECT_EQ(failure->alignment, 32);
    EXPECT_EQ(failure->claimed_bytes, 128);
    EXPECT_EQ(failure->total_capacity_bytes, 512);
}

TEST_F(RootArenaTest, MovingABlockTransfersItsRange) {
    auto source{arena_.try_acquire_block(128, 64)};
    ASSERT_TRUE(source);
    auto* const address{source.data()};
    Block destination{std::move(source)};

    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_FALSE(source);
    EXPECT_EQ(destination.data(), address);
    EXPECT_EQ(arena_.get_statistics().live_block_count, 1);

    destination = {};
    EXPECT_EQ(arena_.get_statistics().live_block_count, 0);
    auto reused{arena_.try_acquire_block(128, 64)};
    ASSERT_TRUE(reused);
    EXPECT_EQ(reused.data(), address);
}

TEST_F(RootArenaTest, PmrDeallocationReturnsTheRange) {
    auto& resource{arena_.memory_resource()};
    auto* const first{resource.allocate(128, 64)};
    resource.deallocate(first, 128, 64);
    auto* const second{resource.allocate(128, 64)};
    EXPECT_EQ(second, first);
    resource.deallocate(second, 128, 64);
}
}
