#pragma once

#include <Containers/ArrayView.h>
#include <CoreMinimal.h>
#include <Sandbox/batch_game/TestTeam.h>

namespace ml {
struct TestSimulationDriver;
struct FSoftTestAssertions;

void check_all_teams_are(TConstArrayView<ETestTeam> teams,
                         ETestTeam expected_team,
                         FSoftTestAssertions& checks,
                         FString const& description);
void check_health_decreased(int32 before,
                            int32 after,
                            FSoftTestAssertions& checks,
                            FString const& description);
void check_samples_recorded(int32 sample_count,
                            FSoftTestAssertions& checks,
                            FString const& description);
void check_radii(TConstArrayView<float> radii, FSoftTestAssertions& checks, float threshold);
void check_radii(TestSimulationDriver const& driver, FSoftTestAssertions& checks, float threshold);
}
