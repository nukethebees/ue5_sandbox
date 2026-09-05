#pragma once

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/DrawDebugConfig.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

struct SPACEGAME_API FLaserPresentation {
    friend struct FLevelPresentation;
    friend struct FLevelSimulation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{5};

    explicit FLaserPresentation(UInstancedStaticMeshComponent& component);

    auto get_config() const noexcept -> FLaserProjectileConfig const* { return actor_config; }
    void set_actor_config(FLaserProjectileConfig const* new_config) noexcept {
        actor_config = new_config;
    }
  private:
    void bind_simulation(ml::test_lasers::Simulation& new_simulation);
    auto simulation() -> ml::test_lasers::Simulation&;
    auto simulation() const -> ml::test_lasers::Simulation const&;

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void apply_simulation_changes_to_ismc();
    void prepare_ismc_transforms();
    void update_ismc();
    void spawn_hit_effects();
    void validate_array_sizes() const;

    FLaserProjectileConfig const* actor_config{nullptr};

    UInstancedStaticMeshComponent* instances{nullptr};

    TArray<FInstancedStaticMeshInstanceData> ismc_data;
    TArray<FTransform> dummy_transforms_spawn_buffer;

    bool have_warned_hit_effect{false};

#if WITH_EDITORONLY_DATA
    FDrawDebugConfig debug_drawer;

    bool debugging_shapes_enabled{false};
#endif

    ml::test_lasers::Simulation* bound_simulation{nullptr};
};
