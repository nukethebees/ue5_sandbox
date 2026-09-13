#include "level_checks.h"

#include <SandboxTests/support/SoftTestAssertions.h>

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

}
