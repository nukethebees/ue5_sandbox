#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/phase_checks.h>
#include <ioj/sim/sim_clock.h>

#include <gtest/gtest.h>

namespace ioj::sim::tests {
static_assert(sizeof(EntityInstanceHandle) == sizeof(std::uint32_t));
static_assert(EntityInstanceHandle::index_layout.bits == 18);
static_assert(EntityInstanceHandle::team_layout.offset == 18);
static_assert(EntityInstanceHandle::health_state_layout.offset == 21);

TEST(EntityInstanceHandle, RepresentsTheMaximumRowWithoutTouchingReservedBits) {
    EntityInstanceHandle result;
    EXPECT_FALSE(result.is_valid());
    result = EntityInstanceHandle{262143, Team::Yellow, 3};
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(result.index(), 262143u);
    EXPECT_EQ(result.raw_value() >> 23, 0u);
}

TEST(EntityInstanceHandle, RowRepairPreservesPublishedMetadata) {
    EntityInstanceHandle handle{17, Team::Green, 1};
    handle.set_index(2);
    EXPECT_EQ(handle.index(), 2u);
    EXPECT_EQ(handle.team(), Team::Green);
    EXPECT_EQ(handle.health_state(), 1);
    handle.set_health_state(0);
    EXPECT_EQ(handle.index(), 2u);
    EXPECT_EQ(handle.team(), Team::Green);
}

TEST(SimulationPhases, RejectsBackwardsTransitionsAndSkippedStages) {
    EXPECT_FALSE(SimClock::permits_transition(SimulationPhase::ResolutionCommit,
                                              SimulationPhase::Resolution));
    EXPECT_FALSE(
        SimClock::permits_transition(SimulationPhase::Thinking, SimulationPhase::Preparation));
    EXPECT_FALSE(
        SimClock::permits_transition(SimulationPhase::Thinking, SimulationPhase::Resolution));
    EXPECT_FALSE(
        SimClock::permits_transition(SimulationPhase::BetweenTicks, SimulationPhase::Thinking));
}

TEST(SimulationPhases, AllowsStableSetupAndSuccessiveTicks) {
    LevelState level_state{LevelState::Uninitialised};
    SimClock clock;
    EXPECT_FALSE(is_lookup_allowed(clock.phase(), level_state));
    EXPECT_TRUE(is_preparation_mutation_allowed(clock));
    EXPECT_EQ(clock.phase(), SimulationPhase::Preparation);
    level_state = LevelState::StableSetup;
    EXPECT_TRUE(is_lookup_allowed(clock.phase(), level_state));
    clock.advance_to(SimulationPhase::Thinking);
    EXPECT_TRUE(is_lookup_allowed(clock));
    clock.tick_loop.accumulator = 1.0;
    clock.completed_ticks = SimTick{17};
    clock.reset();
    EXPECT_EQ(clock.phase(), SimulationPhase::BetweenTicks);
    EXPECT_EQ(clock.get_completed_ticks(), SimTick{});
    EXPECT_EQ(clock.tick_loop.accumulator, 0.0);
    level_state = LevelState::Paused;
    EXPECT_FALSE(is_lookup_allowed(clock.phase(), level_state));
    EXPECT_FALSE(is_structural_mutation_allowed(clock));
    level_state = LevelState::Running;
    for (int tick{}; tick < 2; ++tick) {
        clock.transition_to(SimulationPhase::Preparation);
        EXPECT_FALSE(is_lookup_allowed(clock));
        EXPECT_TRUE(is_preparation_mutation_allowed(clock));
        clock.transition_to(SimulationPhase::Thinking);
        EXPECT_TRUE(is_lookup_allowed(clock));
        EXPECT_FALSE(is_structural_mutation_allowed(clock));
        clock.transition_to(SimulationPhase::Action);
        EXPECT_TRUE(is_lookup_allowed(clock));
        EXPECT_FALSE(is_structural_mutation_allowed(clock));
        clock.transition_to(SimulationPhase::Resolution);
        EXPECT_TRUE(is_lookup_allowed(clock));
        EXPECT_FALSE(is_structural_mutation_allowed(clock));
        clock.transition_to(SimulationPhase::ResolutionCommit);
        EXPECT_FALSE(is_lookup_allowed(clock));
        EXPECT_TRUE(is_structural_mutation_allowed(clock));
        clock.transition_to(SimulationPhase::BetweenTicks);
        EXPECT_FALSE(is_lookup_allowed(clock));
        EXPECT_FALSE(is_structural_mutation_allowed(clock));
    }

    clock.advance_to(SimulationPhase::Thinking);
    clock.advance_to(SimulationPhase::Thinking);
    EXPECT_EQ(clock.phase(), SimulationPhase::Thinking);
    EXPECT_EQ(clock.get_completed_ticks(), SimTick{});
    level_state = LevelState::Paused;
    EXPECT_FALSE(is_lookup_allowed(clock.phase(), level_state));
    level_state = LevelState::Stopped;
    EXPECT_FALSE(is_lookup_allowed(clock.phase(), level_state));
}

}
