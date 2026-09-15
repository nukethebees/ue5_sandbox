#include <ioj/sim/memory/game_memory.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

TEST(NativeSimulation, GameMemoryTest) {
    GameMemory memory{{.root_capacity_bytes = 512}};
    tests::expect_true(memory.owns_backing_locally(),
                       "Direct game memory owns its backing locally");

    std::byte* first_address{};
    {
        auto block{memory.acquire_block(128, 64)};
        first_address = block.data();
        tests::expect_equal(memory.get_stats().live_block_bytes,
                            std::size_t{128},
                            "A live block contributes its requested bytes");
    }
    auto reused{memory.acquire_block(128, 64)};
    tests::expect_equal(reused.data(), first_address, "A returned exact block is reused");

    auto exhaustion{memory.try_acquire_block(1024, 32)};
    tests::expect_false(exhaustion.has_value(), "Root exhaustion is nonfatal through the try API");
    auto const failure{memory.get_last_allocation_failure()};
    tests::expect_true(failure.has_value(), "Root exhaustion records a diagnostic");
    tests::expect_equal(
        failure->requested_bytes, std::size_t{1024}, "Diagnostic records requested bytes");
    tests::expect_equal(failure->alignment, std::size_t{32}, "Diagnostic records alignment");
    tests::expect_equal(
        failure->claimed_bytes, std::size_t{128}, "Diagnostic records bytes already claimed");
    tests::expect_equal(
        failure->total_capacity_bytes, std::size_t{512}, "Diagnostic records total capacity");

    auto backing{GameMemoryBacking::create(1024)};
    auto first_lease{backing->try_acquire_lease()};
    tests::expect_true(first_lease.has_value(), "The first backing lease succeeds");
    tests::expect_false(backing->try_acquire_lease().has_value(),
                        "A simultaneous backing lease fails");
    auto* const backing_address{first_lease->get().data()};

    auto moved_lease{std::move(first_lease.value())};
    tests::expect_false(static_cast<bool>(first_lease.value()), "A moved-from lease is empty");
    first_lease.reset();
    tests::expect_true(backing->is_leased(), "The moved lease keeps the backing occupied");
    moved_lease = {};

    auto second_lease{backing->try_acquire_lease()};
    tests::expect_true(second_lease.has_value(), "A sequential backing lease succeeds");
    tests::expect_equal(second_lease->get().data(),
                        backing_address,
                        "Sequential leases expose the persistent address");

    second_lease.reset();
    {
        auto external_lease{backing->try_acquire_lease()};
        tests::expect_true(external_lease.has_value(), "A lease for external game memory succeeds");
        GameMemory external_memory{std::move(external_lease.value()), {.root_capacity_bytes = 256}};
        tests::expect_false(external_memory.owns_backing_locally(),
                            "External game memory does not own its backing locally");
        tests::expect_equal(external_memory.backing_address(),
                            backing_address,
                            "External game memory uses the leased backing");
    }
    tests::expect_false(backing->is_leased(), "Destroying external game memory releases its lease");

    return;
}

} // namespace tests
