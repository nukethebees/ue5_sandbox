#include "test_setup.h"

#include <Sandbox/core/SandboxDeveloperSettings.h>
#include <SandboxTests/cqtests/SoftTestAssertions.h>

#include <Components/MapTestSpawner.h>
#include <CoreMinimal.h>

namespace ml {
auto level_test_setup(FString const& map_directory,
                      FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner> {
    auto spawner{MakeUnique<FMapTestSpawner>(map_directory, map_name)};
    spawner->AddWaitUntilLoadedCommand(test_runner);

    checks.test_runner = test_runner;
    checks.all_passed = true;

#if WITH_EDITOR
    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    checks.log_successful_assertions = settings->log_successful_assertions;
#endif

    return spawner;
}

auto level_test_setup(FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner> {
    return ml::level_test_setup(
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
        map_name,
        test_runner,
        checks);
}
}
