#include "support/simulation_test_support.h"
#include <ioj/sim/memory/game_memory.h>

namespace ioj::sim::tests {

TEST(NativeSimulation, GameMemoryTest) {
    GameMemory memory{{.root_capacity_bytes = 512}};
    EXPECT_TRUE(memory.owns_backing_locally()) << "Direct game memory owns its backing locally";

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

    auto backing{GameMemoryBacking::create(1024)};
    auto first_lease{backing->try_acquire_lease()};
    EXPECT_TRUE(first_lease.has_value()) << "The first backing lease succeeds";
    EXPECT_FALSE(backing->try_acquire_lease().has_value()) << "A simultaneous backing lease fails";
    auto* const backing_address{first_lease->get().data()};

    auto moved_lease{std::move(first_lease.value())};
    EXPECT_FALSE(static_cast<bool>(first_lease.value())) << "A moved-from lease is empty";
    first_lease.reset();
    EXPECT_TRUE(backing->is_leased()) << "The moved lease keeps the backing occupied";
    moved_lease = {};

    auto second_lease{backing->try_acquire_lease()};
    EXPECT_TRUE(second_lease.has_value()) << "A sequential backing lease succeeds";
    EXPECT_EQ(second_lease->get().data(), backing_address)
        << "Sequential leases expose the persistent address";

    second_lease.reset();
    {
        auto external_lease{backing->try_acquire_lease()};
        EXPECT_TRUE(external_lease.has_value()) << "A lease for external game memory succeeds";
        GameMemory external_memory{std::move(external_lease.value()), {.root_capacity_bytes = 256}};
        EXPECT_FALSE(external_memory.owns_backing_locally())
            << "External game memory does not own its backing locally";
        EXPECT_EQ(external_memory.backing_address(), backing_address)
            << "External game memory uses the leased backing";
    }
    EXPECT_FALSE(backing->is_leased()) << "Destroying external game memory releases its lease";
}

} // namespace tests
