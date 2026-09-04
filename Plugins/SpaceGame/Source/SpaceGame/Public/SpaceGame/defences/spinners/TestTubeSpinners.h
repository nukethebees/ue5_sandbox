#pragma once

#include <SpaceGame/defences/spinners/TestTubeSpinnersSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestTubeSpinners.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class ATestTubeSpinners : public AActor {
    GENERATED_BODY()
    friend class ATestBatchOrchestrator;
  public:
    static constexpr bool is_world_space{false};

    ATestTubeSpinners();
    void set_actor_config(FTubeSpinnerConfig const* new_config) noexcept;
  private:
    void bind_simulation(ml::test_tube_spinners::Simulation& new_simulation);
    auto simulation() -> ml::test_tube_spinners::Simulation&;
    auto simulation() const -> ml::test_tube_spinners::Simulation const&;

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void update_ismc_transforms();
    void update_ismc();
    void validate_array_sizes() const;

    FTubeSpinnerConfig const* actor_config{nullptr};
    ml::test_tube_spinners::Simulation* bound_simulation{nullptr};

    UPROPERTY(meta = (AllowPrivateAccess))
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    UPROPERTY(meta = (AllowPrivateAccess))
    TArray<FTransform> ismc_transforms;
};
