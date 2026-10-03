#include <ioj/sim/entity_instance_handle.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/testing/sim_clock_test_access.h>

#include <gtest/gtest.h>

namespace ioj::sim::tests {
static_assert(sizeof(EntityInstanceHandle) == sizeof(std::uint32_t));
static_assert(EntityInstanceHandle::index_field::bits == 18);
static_assert(EntityInstanceHandle::team_field::offset == 18);
static_assert(EntityInstanceHandle::health_state_field::offset == 21);

TEST(EntityInstanceHandle, RejectsUnrepresentableRowsAndMetadata) {
    EntityInstanceHandle result;
    EXPECT_FALSE(result.is_valid());
#ifndef NDEBUG
    EXPECT_DEATH((EntityInstanceHandle{262144, Team::White, 3}), "");
    EXPECT_DEATH((EntityInstanceHandle{0, static_cast<Team>(ml::enum_count<Team>()), 3}), "");
    EXPECT_DEATH((EntityInstanceHandle{0, Team::White, 4}), "");
#endif
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
    EXPECT_FALSE(SimClock::permits_transition(SimulationPhase::Initialisation,
                                              SimulationPhase::BetweenTicks));
}

TEST(SimulationPhases, AllowsStableSetupAndSuccessiveTicks) {
    SimClock clock;
    EXPECT_FALSE(clock.permits_lookup());
    clock.transition_to(SimulationPhase::StableSetup);
    EXPECT_TRUE(clock.permits_lookup());
    EXPECT_FALSE(clock.permits_structural_mutation());
    clock.transition_to(SimulationPhase::BetweenTicks);
    EXPECT_FALSE(clock.permits_lookup());
    for (int tick{}; tick < 2; ++tick) {
        clock.transition_to(SimulationPhase::Preparation);
        EXPECT_FALSE(clock.permits_lookup());
        EXPECT_TRUE(clock.permits_preparation_mutation());
        clock.transition_to(SimulationPhase::Thinking);
        EXPECT_TRUE(clock.permits_lookup());
        EXPECT_FALSE(clock.permits_structural_mutation());
        clock.transition_to(SimulationPhase::Action);
        EXPECT_TRUE(clock.permits_lookup());
        EXPECT_FALSE(clock.permits_structural_mutation());
        clock.transition_to(SimulationPhase::Resolution);
        EXPECT_TRUE(clock.permits_lookup());
        EXPECT_FALSE(clock.permits_structural_mutation());
        clock.transition_to(SimulationPhase::ResolutionCommit);
        EXPECT_FALSE(clock.permits_lookup());
        EXPECT_TRUE(clock.permits_structural_mutation());
        clock.transition_to(SimulationPhase::BetweenTicks);
        EXPECT_FALSE(clock.permits_lookup());
        EXPECT_FALSE(clock.permits_structural_mutation());
    }
}

struct PhaseLookupPermission {
    SimulationPhase phase;
    bool permitted;
    char const* name;
};

class SimulationLookupPermission : public ::testing::TestWithParam<PhaseLookupPermission> {};

TEST_P(SimulationLookupPermission, PermitsOnlyStableLookupPhases) {
    auto const expectation{GetParam()};
    SimClock clock;
    SimClockTestAccess::set_phase(clock, expectation.phase);

    EXPECT_EQ(clock.permits_lookup(), expectation.permitted);
}

INSTANTIATE_TEST_SUITE_P(
    EveryPhase,
    SimulationLookupPermission,
    ::testing::Values(
        PhaseLookupPermission{SimulationPhase::Initialisation, false, "Initialisation"},
        PhaseLookupPermission{SimulationPhase::StableSetup, true, "StableSetup"},
        PhaseLookupPermission{SimulationPhase::BetweenTicks, false, "BetweenTicks"},
        PhaseLookupPermission{SimulationPhase::Preparation, false, "Preparation"},
        PhaseLookupPermission{SimulationPhase::Thinking, true, "Thinking"},
        PhaseLookupPermission{SimulationPhase::Action, true, "Action"},
        PhaseLookupPermission{SimulationPhase::Resolution, true, "Resolution"},
        PhaseLookupPermission{SimulationPhase::ResolutionCommit, false, "ResolutionCommit"}),
    [](auto const& info) { return info.param.name; });
}
