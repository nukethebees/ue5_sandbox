#pragma once

#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

class UInstancedStaticMeshComponent;

struct SPACEGAMEPRESENTATION_API FTurretPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FTurretPresentation(UInstancedStaticMeshComponent& component);
    void set_actor_config(FTurretConfig const* new_config) noexcept;
  private:
    auto view() const -> FTurretReadView const& { return view_; }
    void ValidateOptionalAssets() const;
    void clear_runtime_state_presentation();
    void begin_play_presentation(TArray<FTransform> initial_transforms);
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void add_initial_visual_instances();
    void add_visual_instances(TArray<FTransform> const& transforms, int32 first_entity_index);
    void configure_ismc();
    void trigger_death_effects();
    void draw_debugging_shapes() const;
    void validate_array_sizes() const;

    FTurretConfig const* actor_config{nullptr};
    FTurretReadView view_{};

    UInstancedStaticMeshComponent* instances{nullptr};
    TArray<FTransform> ismc_transforms;

    FDrawDebugConfig debug_drawer;
    bool draw_target_arrows_enabled{false};
    bool draw_debug_entity_info_enabled{false};
};
