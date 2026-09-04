#pragma once

#include <SpaceGame/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/DrawDebugConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestStaticTurrets.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class SPACEGAME_API ATestStaticTurrets : public AActor {
    GENERATED_BODY()
    friend class ATestBatchOrchestrator;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    ATestStaticTurrets();
    void set_actor_config(FTurretConfig const* new_config) noexcept;
  private:
    void bind_simulation(ml::test_static_turrets::Simulation& new_simulation);
    auto simulation() -> ml::test_static_turrets::Simulation&;
    auto simulation() const -> ml::test_static_turrets::Simulation const&;
    void clear_runtime_state_presentation();
    void begin_play_presentation(TArray<FTransform> initial_transforms);
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void add_initial_visual_instances();
    void configure_ismc();
    void trigger_death_effects();
    void draw_debugging_shapes() const;
    void validate_array_sizes() const;

    FTurretConfig const* actor_config{nullptr};
    ml::test_static_turrets::Simulation* bound_simulation{nullptr};

    UPROPERTY(meta = (AllowPrivateAccess))
    TObjectPtr<UInstancedStaticMeshComponent> instances;
    TArray<FTransform> ismc_transforms;

    UPROPERTY(EditAnywhere, meta = (AllowPrivateAccess))
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    bool draw_target_arrows_enabled{false};
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    bool draw_debug_entity_info_enabled{false};
};
