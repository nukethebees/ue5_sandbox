#include <ioj/sim/player/scalar_response.h>

#include <gtest/gtest.h>

namespace ioj::sim::player::tests {
TEST(ScalarResponse, DirectTracksTargetImmediately) {
    ScalarResponse response;
    ResponseConfig config;

    response.reset(10.f);

    EXPECT_FLOAT_EQ(response.update(0.01f, -5.f, config), -5.f);
}

TEST(ScalarResponse, RateLimitedUsesConfiguredIncreaseAndDecreaseRates) {
    ScalarResponse response;
    ResponseConfig config;
    config.mode = ResponseMode::RateLimited;
    config.rate_limited = {NonNegativeFinite<float>{4.f}, NonNegativeFinite<float>{10.f}};

    response.reset(0.f);
    EXPECT_FLOAT_EQ(response.update(0.5f, 10.f, config), 2.f);
    EXPECT_FLOAT_EQ(response.update(0.5f, 0.f, config), 0.f);
}

TEST(ScalarResponse, SecondOrderRetainsExistingDampedResponse) {
    ScalarResponse response;
    ResponseConfig config;
    config.mode = ResponseMode::SecondOrder;
    config.second_order = {PositiveFinite<float>{3.f}, OpenInterval<float, 0, 1>{0.5f}};

    response.reset(0.f);
    EXPECT_NEAR(response.update(3.f, 1.f, config), 1.0745906f, 1.e-6f);
    EXPECT_NEAR(response.update(7.f, 1.f, config), 1.f, 0.001f);
}

TEST(ScalarResponse, SwitchingModesStartsFromCurrentOutput) {
    ScalarResponse response;
    ResponseConfig rate_limited;
    rate_limited.mode = ResponseMode::RateLimited;
    rate_limited.rate_limited = {NonNegativeFinite<float>{4.f}, NonNegativeFinite<float>{4.f}};

    response.reset(0.f);
    EXPECT_FLOAT_EQ(response.update(0.5f, 10.f, rate_limited), 2.f);

    ResponseConfig second_order;
    second_order.mode = ResponseMode::SecondOrder;
    EXPECT_FLOAT_EQ(response.update(0.f, 10.f, second_order), 2.f);
}
}
