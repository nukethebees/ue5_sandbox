#include <ioj/layout/frame_pacer.hpp>

#include <gtest/gtest.h>

namespace ioj::layout {
namespace {

TEST(FramePacer, SelectsActivityDrivenModes) {
    auto const now{std::chrono::steady_clock::time_point{std::chrono::seconds{10}}};
    FramePacingState state{.last_interaction = now - std::chrono::milliseconds{500}};
    EXPECT_EQ(FramePacer::mode(state, now), FramePacingMode::interactive);

    state.last_interaction = now - std::chrono::seconds{2};
    EXPECT_EQ(FramePacer::mode(state, now), FramePacingMode::idle);

    state.focused = false;
    EXPECT_EQ(FramePacer::mode(state, now), FramePacingMode::background);

    state.minimized = true;
    EXPECT_EQ(FramePacer::mode(state, now), FramePacingMode::suspended);
}

TEST(FramePacer, UsesExpectedWaitIntervals) {
    EXPECT_EQ(FramePacer::wait_timeout(FramePacingMode::interactive), std::chrono::milliseconds{0});
    EXPECT_EQ(FramePacer::wait_timeout(FramePacingMode::idle), std::chrono::milliseconds{83});
    EXPECT_EQ(FramePacer::wait_timeout(FramePacingMode::background), std::chrono::milliseconds{500});
    EXPECT_LT(FramePacer::wait_timeout(FramePacingMode::suspended).count(), 0);
}

} // namespace
} // namespace ioj::layout
