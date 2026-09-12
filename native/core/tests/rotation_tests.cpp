#include <sandbox/core/rotation.h>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <numbers>

namespace {
struct AngleDeltaCase {
    float current;
    float target;
    float expected;
};

struct RotateCase {
    float current;
    float target;
    float speed;
    float delta_time;
    float expected;
};

void expect_normalised_rotation(RotateCase const& test_case) {
    float const current[]{test_case.current};
    float const target[]{test_case.target};
    float out[]{999.0f};
    ml::kernel::rotate_towards_1d_degrees_normalised(
        current, target, test_case.speed, test_case.delta_time, out, 1);
    EXPECT_EQ(out[0], test_case.expected);
}

template <std::floating_point T>
void expect_desired_yaws() {
    constexpr auto pi{std::numbers::pi_v<T>};
    struct Case {
        T start_x;
        T start_y;
        T end_x;
        T end_y;
        T expected;
    };
    Case const cases[]{
        {T{0}, T{0}, T{1}, T{0}, T{0}},
        {T{0}, T{0}, T{0}, T{1}, pi / T{2}},
        {T{0}, T{0}, T{-1}, T{0}, pi},
        {T{0}, T{0}, T{0}, T{-1}, -pi / T{2}},
        {T{10}, T{20}, T{11}, T{21}, pi / T{4}},
        {T{10}, T{20}, T{9}, T{21}, T{3} * pi / T{4}},
        {T{10}, T{20}, T{9}, T{19}, T{-3} * pi / T{4}},
        {T{10}, T{20}, T{11}, T{19}, -pi / T{4}},
    };

    for (auto const& test_case : cases) {
        T const start_x[]{test_case.start_x};
        T const start_y[]{test_case.start_y};
        T const end_x[]{test_case.end_x};
        T const end_y[]{test_case.end_y};
        T out[]{T{123}};
        ml::kernel::compute_desired_yaws_radians(start_x, start_y, end_x, end_y, out, 1);
        EXPECT_NEAR(out[0], test_case.expected, static_cast<T>(1e-6));
    }
}
}

TEST(NativeCoreRotation, ComputesShortestSignedAngleDelta) {
    constexpr AngleDeltaCase cases[]{
        {0.0f, 0.0f, 0.0f},        {90.0f, 90.0f, 0.0f},     {360.0f, 0.0f, 0.0f},
        {0.0f, 10.0f, 10.0f},      {45.0f, 90.0f, 45.0f},    {270.0f, 315.0f, 45.0f},
        {10.0f, 0.0f, -10.0f},     {90.0f, 45.0f, -45.0f},   {315.0f, 270.0f, -45.0f},
        {350.0f, 10.0f, 20.0f},    {10.0f, 350.0f, -20.0f},  {359.0f, 1.0f, 2.0f},
        {1.0f, 359.0f, -2.0f},     {-10.0f, 10.0f, 20.0f},   {10.0f, -10.0f, -20.0f},
        {-350.0f, -10.0f, -20.0f}, {-10.0f, -350.0f, 20.0f}, {0.0f, 370.0f, 10.0f},
        {370.0f, 0.0f, -10.0f},    {720.0f, 90.0f, 90.0f},   {-720.0f, -90.0f, -90.0f},
        {0.0f, 180.0f, -180.0f},   {180.0f, 0.0f, -180.0f},  {90.0f, 270.0f, -180.0f},
        {270.0f, 90.0f, -180.0f},
    };

    for (auto const& test_case : cases) {
        EXPECT_EQ(ml::shortest_signed_angle_delta_degrees(test_case.current, test_case.target),
                  test_case.expected);
    }

    EXPECT_EQ(ml::shortest_signed_angle_delta_degrees(350.0, 10.0), 20.0);
}

TEST(NativeCoreRotation, ComputesNormalisedShortestDelta) {
    constexpr AngleDeltaCase cases[]{
        {0.0f, 0.0f, 0.0f},       {90.0f, 90.0f, 0.0f},     {359.0f, 359.0f, 0.0f},
        {0.0f, 10.0f, 10.0f},     {90.0f, 135.0f, 45.0f},   {270.0f, 315.0f, 45.0f},
        {10.0f, 0.0f, -10.0f},    {135.0f, 90.0f, -45.0f},  {315.0f, 270.0f, -45.0f},
        {0.0f, 270.0f, -90.0f},   {10.0f, 350.0f, -20.0f},  {45.0f, 315.0f, -90.0f},
        {270.0f, 0.0f, 90.0f},    {350.0f, 10.0f, 20.0f},   {315.0f, 45.0f, 90.0f},
        {0.0f, 180.0f, -180.0f},  {90.0f, 270.0f, -180.0f}, {180.0f, 0.0f, -180.0f},
        {270.0f, 90.0f, -180.0f}, {359.0f, 1.0f, 2.0f},     {1.0f, 359.0f, -2.0f},
        {355.0f, 5.0f, 10.0f},    {5.0f, 355.0f, -10.0f},   {0.0f, 360.0f, 0.0f},
        {360.0f, 0.0f, 0.0f},
    };

    for (auto const& test_case : cases) {
        EXPECT_EQ(
            ml::shortest_signed_angle_delta_degrees_normalised(test_case.current, test_case.target),
            test_case.expected);
    }
}

TEST(NativeCoreRotation, RotatesTowardsUnnormalisedAngles) {
    constexpr RotateCase cases[]{
        {0.0f, 0.0f, 90.0f, 1.0f, 0.0f},
        {90.0f, 90.0f, 90.0f, 1.0f, 90.0f},
        {360.0f, 0.0f, 90.0f, 1.0f, 360.0f},
        {0.0f, 90.0f, 30.0f, 1.0f, 30.0f},
        {90.0f, 0.0f, 30.0f, 1.0f, 60.0f},
        {0.0f, 20.0f, 90.0f, 1.0f, 20.0f},
        {20.0f, 0.0f, 90.0f, 1.0f, 0.0f},
        {350.0f, 10.0f, 5.0f, 1.0f, 355.0f},
        {10.0f, 350.0f, 5.0f, 1.0f, 5.0f},
        {350.0f, 10.0f, 90.0f, 1.0f, 10.0f},
        {10.0f, 350.0f, 90.0f, 1.0f, 350.0f},
        {0.0f, 90.0f, 60.0f, 0.5f, 30.0f},
    };

    for (auto const& test_case : cases) {
        float const current[]{test_case.current};
        float const target[]{test_case.target};
        float out[]{-1.0f};
        ml::kernel::rotate_towards_1d_degrees(
            current, target, test_case.speed, test_case.delta_time, out, 1);
        EXPECT_EQ(out[0], test_case.expected);
    }

    std::array<float, 4> const current{0.0f, 90.0f, 350.0f, 10.0f};
    std::array<float, 4> const target{90.0f, 0.0f, 10.0f, 350.0f};
    std::array<float, 4> out{};

    ml::kernel::rotate_towards_1d_degrees(
        current.data(), target.data(), 30.0f, 1.0f, out.data(), 4);

    EXPECT_EQ(out, (std::array<float, 4>{30.0f, 60.0f, 10.0f, 350.0f}));
}

TEST(NativeCoreRotation, RotatesTowardsNormalisedAngles) {
    constexpr RotateCase cases[]{
        {0.0f, 0.0f, 90.0f, 1.0f, 0.0f},         {90.0f, 90.0f, 90.0f, 1.0f, 90.0f},
        {-90.0f, -90.0f, 90.0f, 1.0f, -90.0f},   {-180.0f, -180.0f, 90.0f, 1.0f, -180.0f},
        {0.0f, 90.0f, 30.0f, 1.0f, 30.0f},       {10.0f, 100.0f, 20.0f, 2.0f, 50.0f},
        {-90.0f, 0.0f, 45.0f, 1.0f, -45.0f},     {90.0f, 0.0f, 30.0f, 1.0f, 60.0f},
        {100.0f, 10.0f, 20.0f, 2.0f, 60.0f},     {0.0f, -90.0f, 45.0f, 1.0f, -45.0f},
        {0.0f, 10.0f, 90.0f, 1.0f, 10.0f},       {0.0f, -10.0f, 90.0f, 1.0f, -10.0f},
        {45.0f, 50.0f, 10.0f, 1.0f, 50.0f},      {45.0f, 40.0f, 10.0f, 1.0f, 40.0f},
        {170.0f, -170.0f, 5.0f, 1.0f, 175.0f},   {175.0f, -170.0f, 10.0f, 1.0f, -175.0f},
        {179.0f, -179.0f, 1.0f, 1.0f, -180.0f},  {-170.0f, 170.0f, 5.0f, 1.0f, -175.0f},
        {-175.0f, 170.0f, 10.0f, 1.0f, 175.0f},  {-179.0f, 179.0f, 1.0f, 1.0f, -180.0f},
        {170.0f, -170.0f, 90.0f, 1.0f, -170.0f}, {-170.0f, 170.0f, 90.0f, 1.0f, 170.0f},
        {179.0f, -179.0f, 90.0f, 1.0f, -179.0f}, {-179.0f, 179.0f, 90.0f, 1.0f, 179.0f},
        {0.0f, 90.0f, 0.0f, 1.0f, 0.0f},         {90.0f, 0.0f, 0.0f, 1.0f, 90.0f},
        {170.0f, -170.0f, 0.0f, 1.0f, 170.0f},   {0.0f, 90.0f, 90.0f, 0.0f, 0.0f},
        {90.0f, 0.0f, 90.0f, 0.0f, 90.0f},       {-170.0f, 170.0f, 90.0f, 0.0f, -170.0f},
    };

    for (auto const& test_case : cases) {
        expect_normalised_rotation(test_case);
    }
}

TEST(NativeCoreRotation, SupportsInPlaceAndZeroCount) {
    std::array<float, 4> current{0.0f, 90.0f, 170.0f, -170.0f};
    std::array<float, 4> const target{90.0f, 0.0f, -170.0f, 170.0f};
    ml::kernel::rotate_towards_1d_degrees_normalised_in_place(
        current.data(), target.data(), 10.0f, 1.0f, 4);
    EXPECT_EQ(current, (std::array<float, 4>{10.0f, 80.0f, -180.0f, -180.0f}));

    float out[]{123.0f};
    ml::kernel::rotate_towards_1d_degrees_normalised<float>(nullptr, nullptr, 90.0f, 1.0f, out, 0);
    EXPECT_EQ(out[0], 123.0f);

    ml::kernel::rotate_towards_1d_degrees<float>(nullptr, nullptr, 90.0f, 1.0f, out, 0);
    EXPECT_EQ(out[0], 123.0f);
}

TEST(NativeCoreRotation, RotatesRadians) {
    constexpr auto pi{std::numbers::pi_v<float>};
    float current[]{0.0f};
    float const target[]{pi / 2.0f};
    float out[1]{};

    ml::kernel::rotate_towards_1d_radians(current, target, pi / 4.0f, 1.0f, out, 1);
    EXPECT_NEAR(out[0], 3.0f * pi / 4.0f, 1e-6f);

    ml::kernel::rotate_towards_1d_radians_normalised_in_place(current, target, pi / 4.0f, 1.0f, 1);
    EXPECT_NEAR(current[0], -pi / 4.0f, 1e-6f);
}

TEST(NativeCoreRotation, ComputesDesiredYawForFloatAndDouble) {
    expect_desired_yaws<float>();
    expect_desired_yaws<double>();

    float out[]{123.0f};
    ml::kernel::compute_desired_yaws_radians<float>(nullptr, nullptr, nullptr, nullptr, out, 0);
    EXPECT_EQ(out[0], 123.0f);
}
