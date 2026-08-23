#pragma once

#include "SoftTestAssertions.h"

#include <SandboxCore/error_msg.h>

#include <Components/MapTestSpawner.h>
#include <CoreMinimal.h>

#include <expected>

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

enum class ETestLevelState : uint8 { Unconstructed, Constructing, Constructed };

class FTestBatchOrchestratorLevelSetup {
  public:
    FTestBatchOrchestratorLevelSetup() = default;
    FTestBatchOrchestratorLevelSetup(FMapTestSpawner& spawner,
                                     FAutomationTestBase& test_runner,
                                     FSoftTestAssertions& checks);
    FTestBatchOrchestratorLevelSetup(FTestBatchOrchestratorLevelSetup const&) = delete;
    FTestBatchOrchestratorLevelSetup(FTestBatchOrchestratorLevelSetup&&) = delete;
    auto operator=(FTestBatchOrchestratorLevelSetup const&)
        -> FTestBatchOrchestratorLevelSetup& = delete;
    auto operator=(FTestBatchOrchestratorLevelSetup&&)
        -> FTestBatchOrchestratorLevelSetup& = delete;

    void begin_test(FTestCommandBuilder& command_builder,
                    FAutomationTestBase& test_runner,
                    FSoftTestAssertions& checks);
    void end_test();
    void teardown();
    void setup(FTestCommandBuilder& command_builder,
               FConfigureBatchTestLevel configure_level = {},
               FConfigureBatchTestOrchestrator configure_orchestrator = {});

    auto get_orchestrator() const -> ATestBatchOrchestrator* { return orchestrator.Get(); }
    auto get_config() const -> UTestSimulationConfig const&;
    auto get_world() const -> UWorld&;
    auto get_state() const noexcept -> ETestLevelState { return state; }
    auto get_construction_count() const noexcept -> int32 { return construction_count; }
  private:
    auto spawn_orchestrator(UWorld& world) -> bool;
    auto construct_level() -> bool;

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    FMapTestSpawner* legacy_spawner{nullptr};
    FAutomationTestBase* legacy_test_runner{nullptr};
    FSoftTestAssertions* legacy_checks{nullptr};
    FConfigureBatchTestLevel legacy_configure_level{};
    FConfigureBatchTestOrchestrator legacy_configure_orchestrator{};
    TWeakObjectPtr<ATestBatchOrchestrator> orchestrator{nullptr};
    TObjectPtr<UTestSimulationConfig const> config{nullptr};
    FString map_directory{};
    FString map_name{};
    ETestLevelState state{ETestLevelState::Unconstructed};
    int32 construction_count{0};
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
