#include <SpaceGameSimulation/memory/GameMemory.h>
#include <SpaceGameSimulation/telemetry/LevelTelemetryBlockHistory.h>

#include <Misc/AutomationTest.h>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTelemetryBlockHistoryTest,
                                 "Sandbox.UnitTests.LevelTelemetryBlockHistory",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FLevelTelemetryBlockHistoryTest::RunTest(FString const&) -> bool {
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

    for (uint64 tick{}; tick < 64; ++tick) {
        auto columns{history.append_uninitialized().columns()};
        columns.completed_ticks[0] = tick;
        columns.validity_masks[0] = active_entities_mask;
        columns.active_entities[0] = static_cast<int32>(tick);
    }
    TestEqual(
        TEXT("An exact-capacity fill retains one block"), history.retained_block_count(), int32{1});
    TestEqual(TEXT("An exact-capacity fill has no final unused rows"),
              history.get_stats().unused_samples_in_final_block,
              int32{0});
    auto const* const first_address{history.block_data(0)};
    auto const first_value{history.block_view(0).completed_ticks()[0]};

    auto next{history.append_uninitialized().columns()};
    next.completed_ticks[0] = 64;
    next.validity_masks[0] = active_entities_mask;
    next.active_entities[0] = 64;
    TestEqual(TEXT("One row past capacity acquires exactly one more block"),
              history.retained_block_count(),
              int32{2});
    TestEqual(TEXT("Growth keeps the first payload address stable"),
              history.block_data(0),
              first_address);
    TestEqual(TEXT("Growth keeps the first payload intact"),
              history.block_view(0).completed_ticks()[0],
              first_value);

    uint64 expected_tick{};
    history.for_each_block([this, &expected_tick](auto const block) {
        for (auto const tick : block.completed_ticks()) {
            TestEqual(TEXT("Block iteration remains chronological"), tick, expected_tick);
            ++expected_tick;
        }
    });
    TestEqual(TEXT("Chronological iteration visits every row"), expected_tick, uint64{65});

    history.reset();
    TestEqual(TEXT("Reset clears logical rows"), history.num(), int32{0});
    for (uint64 tick{}; tick < 65; ++tick) {
        auto columns{history.append_uninitialized().columns()};
        columns.completed_ticks[0] = tick;
    }
    TestEqual(TEXT("An equivalent second run acquires no blocks"),
              history.retained_block_count(),
              int32{2});
    TestEqual(
        TEXT("Reset reuse keeps the first payload address"), history.block_data(0), first_address);

    history.reset();
    for (int32 row{}; row < 10; ++row) {
        history.append_uninitialized();
    }
    TestEqual(
        TEXT("A smaller second run acquires no blocks"), history.retained_block_count(), int32{2});

    return true;
}
