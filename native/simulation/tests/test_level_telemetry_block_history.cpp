#include <sandbox/simulation/memory/GameMemory.h>
#include <sandbox/simulation/telemetry/LevelTelemetryBlockHistory.h>
#include "support/simulation_test_support.h"

TEST(NativeSimulation, FLevelTelemetryBlockHistoryTest) {
    using Layout = ml::level_telemetry::FHistoryRowsSingleLayout;
    using Field = ml::level_telemetry::EHistoryField;
    using FieldMask = ml::level_telemetry::FHistoryFieldMask;
    constexpr auto active_entities_mask{[] {
        FieldMask result;
        result.set(Field::ActiveEntities);
        return result;
    }()};
    auto const block_bytes{Layout::layout_bytes(1)};
    FGameMemory memory{{.root_capacity_bytes = block_bytes * 8}};
    FLevelTelemetryBlockHistory history{memory, {.block_bytes = block_bytes}};

    for (ml::simulation::SimTick tick{}; tick < 64; ++tick) {
        auto columns{history.append_uninitialized().columns()};
        columns.completed_ticks[0] = tick;
        columns.validity_masks[0] = active_entities_mask;
        columns.active_entities[0] = static_cast<std::int32_t>(tick);
    }
    ml::simulation_tests::expect_equal(history.retained_block_count(),
                                       std::int32_t{1},
                                       "An exact-capacity fill retains one block");
    ml::simulation_tests::expect_equal(history.get_stats().unused_samples_in_final_block,
                                       std::int32_t{0},
                                       "An exact-capacity fill has no final unused rows");
    auto const* const first_address{history.block_data(0)};
    auto const first_value{history.block_view(0).completed_ticks()[0]};

    auto next{history.append_uninitialized().columns()};
    next.completed_ticks[0] = 64;
    next.validity_masks[0] = active_entities_mask;
    next.active_entities[0] = 64;
    ml::simulation_tests::expect_equal(history.retained_block_count(),
                                       std::int32_t{2},
                                       "One row past capacity acquires exactly one more block");
    ml::simulation_tests::expect_equal(
        history.block_data(0), first_address, "Growth keeps the first payload address stable");
    ml::simulation_tests::expect_equal(history.block_view(0).completed_ticks()[0],
                                       first_value,
                                       "Growth keeps the first payload intact");

    ml::simulation::SimTick expected_tick{};
    history.for_each_block([&expected_tick](auto const block) {
        for (auto const tick : block.completed_ticks()) {
            ml::simulation_tests::expect_equal(
                tick, expected_tick, "Block iteration remains chronological");
            ++expected_tick;
        }
    });
    ml::simulation_tests::expect_equal(
        expected_tick, ml::simulation::SimTick{65}, "Chronological iteration visits every row");

    history.reset();
    ml::simulation_tests::expect_equal(history.num(), std::int32_t{0}, "Reset clears logical rows");
    for (ml::simulation::SimTick tick{}; tick < 65; ++tick) {
        auto columns{history.append_uninitialized().columns()};
        columns.completed_ticks[0] = tick;
    }
    ml::simulation_tests::expect_equal(history.retained_block_count(),
                                       std::int32_t{2},
                                       "An equivalent second run acquires no blocks");
    ml::simulation_tests::expect_equal(
        history.block_data(0), first_address, "Reset reuse keeps the first payload address");

    history.reset();
    for (std::int32_t row{}; row < 10; ++row) {
        history.append_uninitialized();
    }
    ml::simulation_tests::expect_equal(
        history.retained_block_count(), std::int32_t{2}, "A smaller second run acquires no blocks");

    return;
}
