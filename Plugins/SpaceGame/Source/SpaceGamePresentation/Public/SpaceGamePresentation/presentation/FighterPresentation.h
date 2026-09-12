#pragma once

#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

struct SPACEGAMEPRESENTATION_API FFighterPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FFighterPresentation(UInstancedStaticMeshComponent& component);

    void set_actor_config(FFighterConfig const* new_config) noexcept;
  private:
    auto view() const -> FFighterReadView const& { return view_; }

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void apply_simulation_changes_to_ismc();
    void prepare_ismc_transforms();
    void update_ismc();
    void draw_debug_shapes();
    void write_ismc_custom_data(int32 offset, int32 count);
    void validate_array_sizes() const;

    UInstancedStaticMeshComponent* instances{nullptr};

    FFighterConfig const* actor_config{nullptr};
    TArray<FTransform> ismc_transforms;
    TArray<FTransform> dummy_transforms_spawn_buffer;
    TArray<float> custom_data_buffer;

    FDrawDebugConfig debug_drawer;
    bool enable_target_debug_drawing{false};
    bool enable_ship_location_debug_drawing{false};

    FFighterReadView view_{};
};
