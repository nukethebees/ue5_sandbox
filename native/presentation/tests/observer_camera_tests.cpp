#include <ioj/presentation/observer_camera.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>

TEST(NativeObserverCamera, AggregatesTargetsAndLooksBackAtTheirFocus) {
    std::array<ioj::Vector3d, 2> const targets{{{10., 20., 30.}, {30., 40., 50.}}};
    auto const camera{
        ioj::presentation::make_observer_camera_transform(targets, {0., -2., 0.}, 100.)};
    ASSERT_TRUE(camera);
    EXPECT_EQ(camera->focus, (ioj::Vector3d{20., 30., 40.}));
    EXPECT_EQ(camera->transform.location, (ioj::Vector3d{20., -70., 40.}));
    EXPECT_TRUE(ioj::almost_equal(camera->transform.forward(), ioj::Vector3d{0., 1., 0.}, 1.e-12));
}

TEST(NativeObserverCamera, RejectsMissingOrDegenerateInputs) {
    std::array<ioj::Vector3d, 1> targets{{{1., 2., 3.}}};
    auto const make = [&](ioj::Vector3d direction, double distance) {
        return ioj::presentation::make_observer_camera_transform(targets, direction, distance);
    };
    EXPECT_FALSE(ioj::presentation::make_observer_camera_transform({}, {1., 0., 0.}, 1.));
    EXPECT_FALSE(make({}, 1.));
    EXPECT_FALSE(make({1., 0., 0.}, 0.));
    EXPECT_FALSE(make({1., 0., 0.}, -1.));
    EXPECT_FALSE(make({1., 0., 0.}, std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(make({std::numeric_limits<double>::quiet_NaN(), 0., 0.}, 1.));
    targets[0].z() = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(make({1., 0., 0.}, 1.));
}

TEST(NativeObserverCamera, OrbitUsesTheNewPositionToLookAtThePivot) {
    auto const camera{ioj::presentation::orbit_camera_transform({10., 20., 30.}, 90., 100., 0.)};
    ASSERT_TRUE(camera);
    EXPECT_TRUE(ioj::almost_equal(camera->location, ioj::Vector3d{10., 120., 30.}, 1.e-12));
    EXPECT_TRUE(ioj::almost_equal(camera->forward(), ioj::Vector3d{0., -1., 0.}, 1.e-12));
    EXPECT_FALSE(ioj::presentation::orbit_camera_transform({}, 0., -1., 0.));
}
