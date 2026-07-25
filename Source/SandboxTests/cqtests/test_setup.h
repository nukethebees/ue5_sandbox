#pragma once

#include <CoreMinimal.h>

struct FMapTestSpawner;
class FAutomationTestBase;

namespace ml {
struct FSoftTestAssertions;

auto level_test_setup(FString const& map_directory,
                      FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner>;
auto level_test_setup(FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner>;
}
