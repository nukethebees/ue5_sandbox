#include "level_checks.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

namespace ml {
void check_all_teams_are(TConstArrayView<ETestTeam> const teams,
                         ETestTeam const expected_team,
                         FSoftTestAssertions& checks,
                         FString const& description) {
    auto const n{teams.Num()};
    for (int32 i{0}; i < n; ++i) {
        checks.are_equal(expected_team, teams[i], description, i);
    }
}

void check_health_decreased(int32 const before,
                            int32 const after,
                            FSoftTestAssertions& checks,
                            FString const& description) {
    checks.is_true(after < before, description);
}

void check_samples_recorded(int32 const sample_count,
                            FSoftTestAssertions& checks,
                            FString const& description) {
    checks.is_greater_than(sample_count, int32{0}, description);
}

void check_radii(TConstArrayView<float> const radii,
                 FSoftTestAssertions& checks,
                 float const threshold) {
    auto const n{radii.Num()};
    auto const description{FString::Printf(TEXT("Check radii > %.2f"), threshold)};
    for (int32 i{0}; i < n; ++i) {
        checks.is_greater_than(radii[i], threshold, description, i);
    }
}

void check_radii(TestSimulationDriver const& driver,
                 FSoftTestAssertions& checks,
                 float const threshold) {
    auto const& entity_data{driver.registry.get_entity_data()};
    check_radii(TConstArrayView<float>{entity_data.radii}, checks, threshold);
}
}
