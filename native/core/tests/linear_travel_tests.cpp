#include <ioj/linear_travel.h>

#include <gtest/gtest.h>

namespace ioj::tests {
TEST(LinearTravel, ClampsAtEndpointsAndReversesAfterThePause) {
    LinearTravelState state{.start = {1., 2., 3.}, .end = {4., 6., 3.}};
    auto step{advance_linear_travel(state, state.start, 2.5, 1., .5)};
    EXPECT_EQ(step.location, (Vector3d{2.5, 4., 3.}));
    EXPECT_FALSE(step.arrived);
    EXPECT_TRUE(state.towards_end);

    step = advance_linear_travel(state, step.location, 4., 1., .5);
    EXPECT_EQ(step.location, state.end);
    EXPECT_TRUE(step.arrived);
    EXPECT_FALSE(state.towards_end);
    EXPECT_EQ(state.pause_remaining, .5);

    step = advance_linear_travel(state, step.location, 4., .25, .5);
    EXPECT_EQ(step.location, state.end);
    EXPECT_FALSE(step.arrived);
    EXPECT_EQ(state.pause_remaining, .25);
    step = advance_linear_travel(state, step.location, 4., .25, .5);
    EXPECT_EQ(step.location, state.end);
    EXPECT_EQ(state.pause_remaining, 0.);

    step = advance_linear_travel(state, step.location, 10., 1., .5);
    EXPECT_EQ(step.location, state.start);
    EXPECT_TRUE(step.arrived);
    EXPECT_TRUE(state.towards_end);
}

TEST(LinearTravel, ZeroTimeSpeedAndStationaryPathsPreserveLocation) {
    LinearTravelState state{.end = {10., 0., 0.}, .pause_remaining = 1.};
    EXPECT_EQ(advance_linear_travel(state, {}, 10., 0.).location, Vector3d{});
    EXPECT_EQ(state.pause_remaining, 1.);
    state.pause_remaining = 0.;
    auto const stopped{advance_linear_travel(state, {}, 0., 1.)};
    EXPECT_EQ(stopped.location, Vector3d{});
    EXPECT_FALSE(stopped.arrived);
    EXPECT_TRUE(state.towards_end);

    state.end = state.start;
    auto const step{advance_linear_travel(state, state.start, 10., 1.)};
    EXPECT_EQ(step.location, state.start);
    EXPECT_FALSE(step.arrived);
    EXPECT_TRUE(state.towards_end);
}
}
