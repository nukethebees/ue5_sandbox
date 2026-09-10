#pragma once

#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

struct FDelayedNiagaraSpawns;
class UInstancedStaticMeshComponent;

struct SPACEGAMEPRESENTATION_API FCapitalPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FCapitalPresentation(UInstancedStaticMeshComponent& component);

    void set_actor_config(FCapitalShipConfig const* new_config) noexcept;
  private:
    auto view() const -> FCapitalReadView const& { return view_; }
    void set_niagara_spawner(FDelayedNiagaraSpawns& spawner);
    void ValidateOptionalAssets() const;

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void add_initial_visual_instances();
    void add_visual_instances(int32 first_index, int32 count);
    void trigger_death_effects();
    void draw_debugging_shapes() const;
    void visual_log_state() const;
    void validate_array_sizes() const;

    FCapitalShipConfig const* actor_config{nullptr};

    UInstancedStaticMeshComponent* instances{nullptr};
    FDelayedNiagaraSpawns* niagara_spawner{nullptr};

    FDrawDebugConfig debug_drawer;
    bool debugging_shapes_enabled{false};

    FCapitalReadView view_{};
};
