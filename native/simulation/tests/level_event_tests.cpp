#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory_resource>

namespace ioj::sim::tests {
TEST(LevelEvents, ConsumptionPreservesFifoHistoryAndIndependentStreams) {
    static_assert(sizeof(EventHandle) == sizeof(std::uint32_t));
    CompiledLevelEvents data;
    auto const spawn_first{data.schedule.add_spawn_event({0, 1})};
    auto const spawn_second{data.schedule.add_spawn_event({1, 1})};
    auto const stop{data.schedule.add_stop_event(telemetry::RunEndReason::DurationReached)};
    EventHandle const pause{0, EventKind::Pause};
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    EXPECT_FALSE(manager.consume_tick_end_event());
    EXPECT_FALSE(manager.consume_preparation_event());

    for (auto const event : std::array{pause, spawn_second, stop, spawn_first}) {
        manager.enqueue(event);
    }
    EXPECT_EQ(manager.consume_preparation_event(), spawn_second);
    EXPECT_EQ(manager.consume_tick_end_event(), pause);
    EXPECT_TRUE(std::ranges::equal(manager.pending_preparation_events(), std::array{spawn_first}));
    EXPECT_TRUE(std::ranges::equal(manager.pending_tick_end_events(), std::array{stop}));
    EXPECT_EQ(manager.consume_tick_end_event(), stop);
    EXPECT_FALSE(manager.consume_tick_end_event());
    EXPECT_EQ(manager.consume_preparation_event(), spawn_first);
    EXPECT_FALSE(manager.consume_preparation_event());

    manager.enqueue(pause);
    EXPECT_EQ(manager.consume_tick_end_event(), pause);
    EXPECT_FALSE(manager.consume_tick_end_event());
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{pause, stop, pause}));
    EXPECT_TRUE(
        std::ranges::equal(manager.preparation_events(), std::array{spawn_second, spawn_first}));
}

TEST(LevelEvents, MixedTriggersRouteOnceAndQueuesDrainIndependently) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    ExprManager expressions;
    Timers timers;
    Mission mission;
    EntityLedger ledger;
    CompiledLevelEvents data;
    auto const spawn{data.schedule.add_spawn_event({7, 2})};
    EventHandle const pause{0, EventKind::Pause};
    std::array const early{spawn, pause};
    std::array const late{data.schedule.add_stop_event(telemetry::RunEndReason::DurationReached)};
    ASSERT_TRUE(
        data.schedule.add_timed_trigger(expressions, timers, clock, Duration::make(0.2), early));
    ASSERT_TRUE(
        data.schedule.add_timed_trigger(expressions, timers, clock, Duration::make(0.3), late));
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    manager.activate_trigger_expressions(expressions);
    for (auto const timer : manager.data().schedule.initial_timers) {
        timers.start(timer, SimTick{});
    }

    expressions.evaluate({SimTick{1}, ledger, timers, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.preparation_events().empty());
    EXPECT_TRUE(manager.tick_end_events().empty());

    expressions.evaluate({SimTick{2}, ledger, timers, {}}, mission);
    manager.collect_ready(expressions);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{pause}));
    EXPECT_EQ(manager.consume_tick_end_event(), pause);
    EXPECT_FALSE(manager.consume_tick_end_event());

    expressions.evaluate({SimTick{3}, ledger, timers, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.pending_tick_end_events(), late));
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_EQ(manager.consume_tick_end_event(), late.front());
    EXPECT_EQ(manager.consume_preparation_event(), spawn);
    expressions.evaluate({SimTick{4}, ledger, timers, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.pending_preparation_events().empty());
    EXPECT_TRUE(manager.pending_tick_end_events().empty());
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{pause, late.front()}));
}

}
