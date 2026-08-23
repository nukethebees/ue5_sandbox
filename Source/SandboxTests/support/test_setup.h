#pragma once

#include "SoftTestAssertions.h"

#include <SandboxCore/error_msg.h>

#include <Components/MapTestSpawner.h>
#include <CoreMinimal.h>

#include <expected>

struct FMapTestSpawner;
class FAutomationTestBase;
class FTestCommandBuilder;
class UTestSimulationConfig;
class UWorld;
class AActor;
class ATestBatchOrchestrator;

namespace ml {
using FConfigureBatchTestLevel = TFunction<void(UWorld&, UTestSimulationConfig const&)>;
using FConfigureBatchTestOrchestrator =
    TFunction<void(UWorld&, UTestSimulationConfig const&, ATestBatchOrchestrator&)>;

class FTestBatchOrchestratorLevelSetup {
  public:
    FTestBatchOrchestratorLevelSetup(FMapTestSpawner& spawner,
                                     FAutomationTestBase& test_runner,
                                     FSoftTestAssertions& checks);
    ~FTestBatchOrchestratorLevelSetup();

    FTestBatchOrchestratorLevelSetup(FTestBatchOrchestratorLevelSetup const&) = delete;
    FTestBatchOrchestratorLevelSetup(FTestBatchOrchestratorLevelSetup&&) = delete;
    auto operator=(FTestBatchOrchestratorLevelSetup const&)
        -> FTestBatchOrchestratorLevelSetup& = delete;
    auto operator=(FTestBatchOrchestratorLevelSetup&&)
        -> FTestBatchOrchestratorLevelSetup& = delete;

    void setup(FTestCommandBuilder& command_builder,
               FConfigureBatchTestLevel configure_level = {},
               FConfigureBatchTestOrchestrator configure_orchestrator = {});
    void teardown();

    auto get_orchestrator() const -> ATestBatchOrchestrator* { return orchestrator; }
    auto get_world() const -> UWorld&;
  private:
    auto spawn_orchestrator(UWorld& world) -> bool;
    void resolve_orchestrator();

    FMapTestSpawner* spawner{nullptr};
    FAutomationTestBase* test_runner{nullptr};
    FSoftTestAssertions* checks{nullptr};
    ATestBatchOrchestrator* orchestrator{nullptr};
    FDelegateHandle map_change_handle{};
    FConfigureBatchTestLevel configure_level{};
    FConfigureBatchTestOrchestrator configure_orchestrator{};
    bool actors_spawned{false};
};

auto level_test_setup(FString const& map_directory,
                      FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner>;
auto level_test_setup(FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner>;

auto get_editor_world() -> std::expected<UWorld*, FErrorMsg>;
auto spawn_line_of_sight_blocker(UWorld& world, FTransform const& transform) -> AActor*;
}
