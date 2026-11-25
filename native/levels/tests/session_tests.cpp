#include <ioj/levels/launch_options.h>
#include <ioj/session/modal_resume_state.h>

#include <gtest/gtest.h>

#include <limits>

namespace ioj::session::tests {
using namespace levels;
TEST(LaunchOptions, ValidatesTimeScaleAndOptionalDuration) {
    for (auto const value : {0.25, 1.0, 100.0}) {
        EXPECT_TRUE(launch::TimeScale::try_make(value));
    }
    for (auto const value : {0.0,
                             -1.0,
                             100.0001,
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_FALSE(launch::TimeScale::try_make(value));
    }
    for (auto const mode : {PresentationMode::Visual, PresentationMode::SimulationOnly}) {
        LaunchOptions options{.presentation_mode = mode};
        EXPECT_FALSE(options.simulated_duration_seconds);
        options.simulated_duration_seconds = PositiveFinite<double>{2.0};
        EXPECT_EQ(options.simulated_duration_seconds->get(), 2.0);
    }
    for (auto const value : {0.0,
                             -1.0,
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_FALSE(PositiveFinite<double>::try_make(value));
    }
}

TEST(ModalResumeState, PreservesObserverContextAndTracksPlayerAvailabilityDuringSuspension) {
    ModalResumeState state;
    EXPECT_EQ(state.context(ControlContext::Player), ControlContext::Player);
    state.suspend(ControlContext::Observer);
    state.on_player_changed(false);
    EXPECT_EQ(state.context(ControlContext::Player), ControlContext::Observer);
    state.on_player_changed(true);
    EXPECT_EQ(state.context(ControlContext::Observer), ControlContext::Player);
    state.on_player_changed(false);
    EXPECT_EQ(state.context(ControlContext::Observer), ControlContext::None);
    state.clear();
    EXPECT_FALSE(state.pending());
    state.on_player_changed(true);
    EXPECT_EQ(state.context(ControlContext::Benchmark), ControlContext::Benchmark);
    state.suspend(ControlContext::None);
    EXPECT_TRUE(state.pending());
    EXPECT_EQ(state.context(ControlContext::Benchmark), ControlContext::None);
}
}
