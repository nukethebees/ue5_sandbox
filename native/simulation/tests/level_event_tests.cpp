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

TEST(LevelEvents, ResolvingTriggersDeliversEachEventOnceAndRetiresTheExpression) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    ExprManager expressions;
    Mission mission;
    EntityLedger ledger;
    CompiledLevelEvents data;
    std::array const early{data.schedule.add_spawn_event({7, 2}),
                           data.schedule.add_spawn_event({2, 1})};
    std::array const late{data.schedule.add_stop_event(telemetry::RunEndReason::DurationReached)};
    EXPECT_EQ(data.schedule.spawn_events[early[0].index()], (IndexSpan{7, 2}));
    EXPECT_EQ(data.schedule.spawn_events[early[1].index()], (IndexSpan{2, 1}));
    EXPECT_EQ(early[0].kind(), EventKind::Spawn);
    EXPECT_EQ(early[1].kind(), EventKind::Spawn);
    EXPECT_EQ(late[0].kind(), EventKind::StopRun);
    EXPECT_EQ(data.schedule.stop_events[late[0].index()], telemetry::RunEndReason::DurationReached);
    ASSERT_TRUE(data.schedule.add_timed_trigger(expressions, clock, Duration::make(0.2), early));
    ASSERT_TRUE(data.schedule.add_timed_trigger(expressions, clock, Duration::make(0.3), late));
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    manager.activate(expressions);

    expressions.evaluate({SimTick{1}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.preparation_events().empty());
    EXPECT_TRUE(manager.tick_end_events().empty());

    expressions.evaluate({SimTick{2}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), early));
    EXPECT_TRUE(manager.tick_end_events().empty());
    EXPECT_EQ(manager.data().schedule.pending_triggers.size(), 1);
    EXPECT_EQ(expressions.leaves().active.size(), 1);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), early));

    expressions.evaluate({SimTick{3}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), late));
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), early));
    EXPECT_TRUE(manager.data().schedule.pending_triggers.empty());
    EXPECT_TRUE(expressions.leaves().active.empty());
    manager.clear_tick_end_events();
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), early));
    manager.clear_preparation_events();
    expressions.evaluate({SimTick{4}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.preparation_events().empty());
    EXPECT_TRUE(manager.tick_end_events().empty());
}

TEST(LevelEvents, RoutesMixedTriggerEventsToTheirPhases) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    ExprManager expressions;
    Mission mission;
    EntityLedger ledger;
    CompiledLevelEvents data;
    EventHandle const pause{0, EventKind::Pause};
    auto const spawn{data.schedule.add_spawn_event({0, 1})};
    auto const stop{data.schedule.add_stop_event(telemetry::RunEndReason::DurationReached)};
    std::array const events{spawn, pause, stop};
    ASSERT_TRUE(data.schedule.add_timed_trigger(expressions, clock, Duration::make(0.1), events));
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    manager.activate(expressions);

    expressions.evaluate({SimTick{1}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{pause, stop}));
    for (auto const event : manager.tick_end_events()) {
        EXPECT_TRUE(is_tick_end(event.kind()));
        EXPECT_FALSE(is_preparation(event.kind()));
    }
    EXPECT_TRUE(is_preparation(spawn.kind()));
    EXPECT_FALSE(is_tick_end(spawn.kind()));
}
}
