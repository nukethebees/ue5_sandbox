#pragma once
#include <ioj/presentation/mission_frame.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

class UInstancedStaticMeshComponent;

namespace ioj {
struct FLevelPresentation;
} // namespace ioj

namespace ioj {
struct SPACEGAMEPRESENTATION_API FTurretPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FTurretPresentation(UInstancedStaticMeshComponent& component);
    void set_actor_config(FTurretConfig const* new_config,
                          settings::TurretPresentation const& settings) noexcept;
  private:
    void ValidateOptionalAssets() const;
    void clear_runtime_state_presentation();
    void begin_play_presentation(presentation::MissionFrame const& frame);
    void update_visual_data(presentation::FrameEvents const& events);
    void commit_visual_data(presentation::MissionFrame const& frame);
    void end_tick_presentation(presentation::MissionFrame const& frame);

    void add_initial_visual_instances(presentation::MissionFrame const& frame);
    void add_visual_instances(presentation::MissionFrame const& frame,
                              TArray<FTransform> const& transforms,
                              int32 first_entity_index);
    void configure_ismc();
    void trigger_death_effects(presentation::FrameEvents const& events);
    void draw_debugging_shapes(presentation::MissionFrame const& frame) const;
    void validate_array_sizes(presentation::MissionFrame const& frame) const;

    FTurretConfig const* actor_config{nullptr};
    settings::TurretPresentation const* settings_{};

    UInstancedStaticMeshComponent* instances{nullptr};
    TArray<FTransform> ismc_transforms;

    FDrawDebugConfig debug_drawer;
    bool draw_target_arrows_enabled{false};
    bool draw_debug_entity_info_enabled{false};
};

} // namespace ioj
