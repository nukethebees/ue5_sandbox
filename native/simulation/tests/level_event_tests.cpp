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
    EventHandle const spawn{0, EventKind::Spawn};
    std::array events{spawn, stop, mission};
    std::ranges::sort(events);
    EXPECT_EQ(events, (std::array{mission, stop, spawn}));
}

TEST(LevelEvents, ResolvingTriggersDeliversEachEventOnceAndRetiresTheExpression) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    ExprManager expressions;
    Mission mission;
    EntityLedger ledger;
    CompiledLevelEvents data;
    std::array const early{EventHandle{7, EventKind::Spawn}, EventHandle{2, EventKind::Spawn}};
    std::array const late{EventHandle{0, EventKind::StopRun}};
    ASSERT_TRUE(data.schedule.add_timed_trigger(expressions, clock, Duration::make(0.2), early));
    ASSERT_TRUE(data.schedule.add_timed_trigger(expressions, clock, Duration::make(0.3), late));
    LevelEventManager manager{std::move(data), *std::pmr::get_default_resource()};
    manager.activate(expressions);

    expressions.evaluate({SimTick{1}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.ready_events().empty());

    expressions.evaluate({SimTick{2}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.ready_events(), early));
    EXPECT_EQ(manager.data().schedule.pending_triggers.size(), 1);
    EXPECT_EQ(expressions.leaves().active.size(), 1);
    manager.clear_ready();
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.ready_events().empty());

    expressions.evaluate({SimTick{3}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(std::ranges::equal(manager.ready_events(), late));
    EXPECT_TRUE(manager.data().schedule.pending_triggers.empty());
    EXPECT_TRUE(expressions.leaves().active.empty());
    manager.clear_ready();
    expressions.evaluate({SimTick{4}, ledger, {}}, mission);
    manager.collect_ready(expressions);
    EXPECT_TRUE(manager.ready_events().empty());
}
}
