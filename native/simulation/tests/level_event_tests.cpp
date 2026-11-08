#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/expressions/latches.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/sim/levels/level_event_manager.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory_resource>

namespace ioj::sim::tests {
TEST(LevelEvents, ConsumptionPreservesFifoHistoryAndIndependentStreams) {
    static_assert(sizeof(EventHandle) == sizeof(std::uint32_t));
    CompiledLevelEvents data;
    auto const spawn_first{data.add_spawn_event({0, 1})};
    auto const spawn_second{data.add_spawn_event({1, 1})};
    auto const stop{data.add_stop_event(telemetry::RunEndReason::DurationReached)};
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
    ExprManager expressions;
    Timers timers;
    Latches latches;
    EntityLedger ledger;
    CompiledLevelEvents data;
    auto const spawn{data.add_spawn_event({7, 2})};
    EventHandle const pause{0, EventKind::Pause};
    std::array const early{spawn, pause};
    std::array const late{data.add_stop_event(telemetry::RunEndReason::DurationReached)};
    auto const early_timer{timers.add()};
    auto const late_timer{timers.add()};
    data.initial_events = {data.add_start_timer_event({early_timer, 2}),
                           data.add_start_timer_event({late_timer, 3})};
    data.add_trigger(expressions.add_timer_elapsed(early_timer), early);
    data.add_trigger(expressions.add_timer_elapsed(late_timer), late);
    auto const initial_events{data.initial_events};
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    for (auto const handle : initial_events) {
        ASSERT_EQ(manager.consume_tick_end_event(), handle);
        auto const& event{manager.data().start_timer_events[handle.index()]};
        timers.start(event.timer, event.duration);
    }

    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, latches});
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.preparation_events().empty());
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), initial_events));
    EXPECT_TRUE(manager.pending_tick_end_events().empty());

    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, latches});
    manager.collect_ready(expressions);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(std::ranges::equal(manager.pending_tick_end_events(), std::array{pause}));
    EXPECT_EQ(manager.consume_tick_end_event(), pause);
    EXPECT_FALSE(manager.consume_tick_end_event());

    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, latches});
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.pending_tick_end_events(), late));
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_EQ(manager.consume_tick_end_event(), late.front());
    EXPECT_EQ(manager.consume_preparation_event(), spawn);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, latches});
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.pending_preparation_events().empty());
    EXPECT_TRUE(manager.pending_tick_end_events().empty());
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(
        std::ranges::equal(manager.tick_end_events().first(initial_events.size()), initial_events));
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events().subspan(initial_events.size()),
                                   std::array{pause, late.front()}));
}

}
