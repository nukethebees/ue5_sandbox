#include <ioj/sim/memory/game_memory.h>
#include <ioj/sim/telemetry/level_telemetry_block_history.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

TEST(NativeSimulation, LevelTelemetryBlockHistoryTest) {
    using Layout = ioj::sim::telemetry::HistoryRowsSingleLayout;
    using Field = ioj::sim::telemetry::HistoryField;
    using FieldMask = ioj::sim::telemetry::HistoryFieldMask;
    constexpr auto active_entities_mask{[] {
        FieldMask result;
        result.set(Field::ActiveEntities);
        return result;
    }()};
    auto const block_bytes{Layout::layout_bytes(1)};
    GameMemory memory{{.root_capacity_bytes = block_bytes * 8}};
    LevelTelemetryBlockHistory history{memory, {.block_bytes = block_bytes}};

    for (ioj::sim::SimTick tick{}; tick < 64; ++tick) {
        auto columns{history.append_uninitialized().columns()};
        columns.completed_ticks[0] = tick;
        columns.validity_masks[0] = active_entities_mask;
        columns.active_entities[0] = static_cast<std::int32_t>(tick);
    }
    ioj::sim::tests::expect_equal(history.retained_block_count(),
                                  std::int32_t{1},
                                  "An exact-capacity fill retains one block");
    ioj::sim::tests::expect_equal(history.get_stats().unused_samples_in_final_block,
                                  std::int32_t{0},
                                  "An exact-capacity fill has no final unused rows");
    auto const* const first_address{history.block_data(0)};
    auto const first_value{history.block_view(0).completed_ticks()[0]};

    auto next{history.append_uninitialized().columns()};
    next.completed_ticks[0] = 64;
    next.validity_masks[0] = active_entities_mask;
    next.active_entities[0] = 64;
    ioj::sim::tests::expect_equal(history.retained_block_count(),
                                  std::int32_t{2},
                                  "One row past capacity acquires exactly one more block");
    ioj::sim::tests::expect_equal(
        history.block_data(0), first_address, "Growth keeps the first payload address stable");
    ioj::sim::tests::expect_equal(history.block_view(0).completed_ticks()[0],
                                  first_value,
                                  "Growth keeps the first payload intact");

    ioj::sim::SimTick expected_tick{};
    history.for_each_block([&expected_tick](auto const block) {
        for (auto const tick : block.completed_ticks()) {
            ioj::sim::tests::expect_equal(
                tick, expected_tick, "Block iteration remains chronological");
            ++expected_tick;
        }
    });
    ioj::sim::tests::expect_equal(
        expected_tick, ioj::sim::SimTick{65}, "Chronological iteration visits every row");

    history.reset();
    ioj::sim::tests::expect_equal(history.num(), std::int32_t{0}, "Reset clears logical rows");
    for (ioj::sim::SimTick tick{}; tick < 65; ++tick) {
        auto columns{history.append_uninitialized().columns()};
        columns.completed_ticks[0] = tick;
    }
    ioj::sim::tests::expect_equal(history.retained_block_count(),
                                  std::int32_t{2},
                                  "An equivalent second run acquires no blocks");
    ioj::sim::tests::expect_equal(
        history.block_data(0), first_address, "Reset reuse keeps the first payload address");

    history.reset();
    for (std::int32_t row{}; row < 10; ++row) {
        history.append_uninitialized();
    }
    ioj::sim::tests::expect_equal(
        history.retained_block_count(), std::int32_t{2}, "A smaller second run acquires no blocks");

    return;
}

} // namespace ioj::sim::tests
