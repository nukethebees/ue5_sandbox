#include <ioj/frame_memory_resource.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/entity_lookup_tables.h>
#include <ioj/sim/expressions/condition_manager.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/sim/levels/level_event_manager.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory_resource>
#include <utility>

namespace ioj::sim::tests {
TEST(LevelEvents, ConsumptionPreservesFifoHistoryAndIndependentStreams) {
    static_assert(sizeof(EventHandle) == sizeof(std::uint32_t));
    LevelEventManager data;
    auto const spawn_first{data.add_spawn_event({0, 1})};
    auto const spawn_second{data.add_spawn_event({1, 1})};
    auto const stop{data.add_stop_event(RunEndReason::DurationReached)};
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
    ConditionManager expressions;
    Timers timers;
    EntityLedger ledger;
    EntityLookupTables lookups{};
    SpatialQueryManager spatial{};
    FrameMemoryResource scratch{4096};
    LevelEventManager data;
    auto const spawn{data.add_spawn_event({7, 2})};
    EventHandle const pause{0, EventKind::Pause};
    std::array const early{spawn, pause};
    std::array const late{data.add_stop_event(RunEndReason::DurationReached)};
    auto const early_timer{timers.add()};
    auto const late_timer{timers.add()};
    std::array const initial_events{data.add_start_timer_event({early_timer, 2}),
                                    data.add_start_timer_event({late_timer, 3})};
    for (auto const event : initial_events) {
        data.enqueue(event);
    }
    data.add_trigger(expressions.add_timer_elapsed(early_timer), early);
    data.add_trigger(expressions.add_timer_elapsed(late_timer), late);
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    for (auto const handle : initial_events) {
        ASSERT_EQ(manager.consume_tick_end_event(), handle);
        auto const& event{manager.start_timer_event(handle)};
        timers.start(event.timer, event.duration);
    }

    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.preparation_events().empty());
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), initial_events));
    EXPECT_TRUE(manager.pending_tick_end_events().empty());

    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    manager.collect_ready(expressions);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(std::ranges::equal(manager.pending_tick_end_events(), std::array{pause}));
    EXPECT_EQ(manager.consume_tick_end_event(), pause);
    EXPECT_FALSE(manager.consume_tick_end_event());

    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.pending_tick_end_events(), late));
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_EQ(manager.consume_tick_end_event(), late.front());
    EXPECT_EQ(manager.consume_preparation_event(), spawn);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.pending_preparation_events().empty());
    EXPECT_TRUE(manager.pending_tick_end_events().empty());
    EXPECT_TRUE(std::ranges::equal(manager.preparation_events(), std::array{spawn}));
    EXPECT_TRUE(
        std::ranges::equal(manager.tick_end_events().first(initial_events.size()), initial_events));
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events().subspan(initial_events.size()),
                                   std::array{pause, late.front()}));
}

struct EventConditions {
    void evaluate() { conditions.evaluate({ledger, timers, {}, spatial, lookups, scratch}); }

    ConditionManager conditions;
    Timers timers;
    EntityLedger ledger;
    EntityLookupTables lookups;
    SpatialQueryManager spatial;
    FrameMemoryResource scratch{4096};
};

TEST(LevelEvents, RemovingFiredTriggersPreservesOrderAndNewRegistrationsWaitForCollection) {
    EventConditions samples;
    auto const early{samples.timers.add()};
    auto const late{samples.timers.add()};
    auto const early_condition{samples.conditions.add_timer_elapsed(early)};
    auto const late_condition{samples.conditions.add_timer_elapsed(late)};
    samples.timers.start(early, 1);
    samples.timers.start(late, 2);
    LevelEventManager manager;
    std::array<EventHandle, 5> events;
    for (auto& event : events) {
        event = manager.add_stop_event(RunEndReason::WorldEnd);
    }
    manager.add_trigger(late_condition, std::array{events[0], events[1]});
    manager.add_trigger(early_condition, std::array{events[2]});
    manager.add_trigger(late_condition, std::array{events[3]});
    samples.timers.tick(1);
    samples.evaluate();
    manager.collect_ready(samples.conditions);
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{events[2]}));

    manager.add_trigger(early_condition, std::array{events[4]});
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{events[2]}));
    samples.timers.tick(1);
    samples.evaluate();
    manager.collect_ready(samples.conditions);
    manager.collect_ready(samples.conditions);
    EXPECT_TRUE(
        std::ranges::equal(manager.tick_end_events(),
                           std::array{events[2], events[0], events[1], events[3], events[4]}));
    EXPECT_EQ(manager.triggers().size(), 4);
}

struct RegisterTriggerOnAllocation : std::pmr::memory_resource {
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        if (auto* target = std::exchange(manager, nullptr)) {
            target->add_trigger(condition, std::array{event});
        }
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }

    LevelEventManager* manager{};
    ExprHandle condition;
    EventHandle event;
};

TEST(LevelEvents, RegistrationDuringCollectionBecomesEligibleOnFollowingPass) {
    EventConditions samples;
    auto const timer{samples.timers.add()};
    auto const ready{samples.conditions.add_timer_elapsed(timer)};
    samples.timers.start(timer, 1);
    samples.timers.tick(1);
    samples.evaluate();
    RegisterTriggerOnAllocation memory;
    LevelEventManager manager{LevelEventManager{}, memory};
    auto const first{manager.add_stop_event(RunEndReason::WorldEnd)};
    auto const second{manager.add_stop_event(RunEndReason::DurationReached)};
    auto const added{manager.add_win_event(Team::Blue)};
    manager.add_trigger(ready, std::array{first, second});
    memory.manager = &manager;
    memory.condition = ready;
    memory.event = added;
    manager.collect_ready(samples.conditions);
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{first, second}));
    manager.collect_ready(samples.conditions);
    manager.collect_ready(samples.conditions);
    EXPECT_TRUE(std::ranges::equal(manager.tick_end_events(), std::array{first, second, added}));
}

TEST(LevelEvents, DefinitionGrowthPreservesHandlesAndDynamicStopRequests) {
    LevelEventManager manager;
    auto const spawn{manager.add_spawn_event({4, 2})};
    auto const stop{manager.add_stop_event(RunEndReason::DurationReached)};
    auto const win{manager.add_win_event(Team::Blue)};
    TimerHandle const timer{3};
    auto const start{manager.add_start_timer_event({timer, 7})};
    for (std::uint32_t i{}; i < 512; ++i) {
        manager.add_spawn_event({i, 1});
        manager.add_stop_event(RunEndReason::WorldEnd);
        manager.add_win_event(Team::Red);
        manager.add_start_timer_event({timer, static_cast<TimerTicks>(i)});
    }
    EXPECT_EQ(manager.spawn_event(spawn).offset, 4);
    EXPECT_EQ(manager.spawn_event(spawn).count, 2);
    EXPECT_EQ(manager.stop_event(stop), RunEndReason::DurationReached);
    EXPECT_EQ(manager.win_event(win).winner, Team::Blue);
    EXPECT_EQ(manager.start_timer_event(start).timer, timer);
    EXPECT_EQ(manager.start_timer_event(start).duration, 7);
    manager.enqueue(spawn);
    manager.enqueue(win);
    manager.enqueue_stop(RunEndReason::WorldEnd);
    EXPECT_EQ(manager.consume_preparation_event(), spawn);
    EXPECT_EQ(manager.consume_tick_end_event(), win);
    auto const dynamic_stop{manager.consume_tick_end_event()};
    ASSERT_TRUE(dynamic_stop);
    EXPECT_EQ(manager.stop_event(*dynamic_stop), RunEndReason::WorldEnd);
}

TEST(LevelEvents, MovesPreserveQueuedEventsAndReadCursors) {
    LevelEventManager data;
    auto const spawn{data.add_spawn_event({0, 1})};
    auto const stop{data.add_stop_event(RunEndReason::WorldEnd)};
    EventHandle const pause{0, EventKind::Pause};
    for (auto const event : std::array{spawn, pause, stop}) {
        data.enqueue(event);
    }
    LevelEventManager first{std::move(data)};
    EXPECT_TRUE(std::ranges::equal(first.pending_tick_end_events(), std::array{pause, stop}));
    EXPECT_TRUE(std::ranges::equal(first.pending_preparation_events(), std::array{spawn}));
    EXPECT_EQ(first.consume_tick_end_event(), pause);
    EXPECT_EQ(first.consume_preparation_event(), spawn);

    std::pmr::monotonic_buffer_resource memory;
    LevelEventManager second{std::move(first), memory};
    LevelEventManager assigned;
    assigned = std::move(second);
    EXPECT_TRUE(std::ranges::equal(assigned.tick_end_events(), std::array{pause, stop}));
    EXPECT_TRUE(std::ranges::equal(assigned.preparation_events(), std::array{spawn}));
    EXPECT_EQ(assigned.consume_tick_end_event(), stop);
    EXPECT_FALSE(assigned.consume_tick_end_event());
    EXPECT_FALSE(assigned.consume_preparation_event());
}

}
