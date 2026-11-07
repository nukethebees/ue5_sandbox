#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory_resource>

namespace ioj::sim::tests {
TEST(LevelEvents, PackedHandlesSortByKindBeforePayloadIndex) {
    static_assert(sizeof(EventHandle) == sizeof(std::uint32_t));
    EventHandle const mission{EventHandle::index_layout.value_mask, EventKind::MissionCompleted};
    EventHandle const stop{EventHandle::index_layout.value_mask, EventKind::StopRun};
    EventHandle const pause{0, EventKind::Pause};
    EventHandle const spawn{0, EventKind::Spawn};
    std::array events{spawn, pause, stop, mission};
    std::ranges::sort(events);
    EXPECT_EQ(events, (std::array{mission, stop, pause, spawn}));
}

TEST(LevelEvents, MixedTriggersRouteOnceAndQueuesDrainIndependently) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    ExprManager expressions;
    Mission mission;
    EntityLedger ledger;
    CompiledLevelEvents data;
    auto const spawn{data.schedule.add_spawn_event({7, 2})};
    EventHandle const pause{0, EventKind::Pause};
    std::array const early{spawn, pause};
    std::array const late{data.schedule.add_stop_event(telemetry::RunEndReason::DurationReached)};
    ASSERT_TRUE(data.schedule.add_timed_trigger(expressions, clock, Duration::make(0.2), early));
    ASSERT_TRUE(data.schedule.add_timed_trigger(expressions, clock, Duration::make(0.3), late));
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    manager.activate_trigger_expressions(expressions);

    expressions.evaluate({SimTick{1}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.preparation_events().empty());
    EXPECT_TRUE(manager.tick_end_events().empty());

    expressions.evaluate({SimTick{2}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{pause}));
    manager.clear_tick_end_events();

    expressions.evaluate({SimTick{3}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), late));
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    manager.clear_tick_end_events();
    manager.clear_preparation_events();
    expressions.evaluate({SimTick{4}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.preparation_events().empty());
    EXPECT_TRUE(manager.tick_end_events().empty());
}

}
