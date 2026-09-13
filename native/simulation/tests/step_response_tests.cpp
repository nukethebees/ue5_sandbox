#include "sandbox/simulation/step_response.h"

#include <gtest/gtest.h>

TEST(StepResponse, MatchesDampedResponseAndApproachesOne) {
    ml::simulation::DampedStepResponse response;
    response.configure(3.0f, 0.5f);

    EXPECT_NEAR(response.value_at(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(response.value_at(3.0f), 1.0745906f, 1e-6f);
    EXPECT_NEAR(response.value_at(10.0f), 1.0f, 0.001f);
}
