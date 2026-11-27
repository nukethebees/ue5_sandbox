#pragma once
#include <ioj/presentation/level_frame.h>
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
    void begin_play_presentation(presentation::AgentDisplayBatch const& entities,
                                 presentation::rotators3f::ConstView rotations);
    void update_visual_data(presentation::InstanceChanges const& changes);
    void commit_visual_data(presentation::AgentDisplayBatch const& entities,
                            presentation::TargetView const& target_view);

    void add_initial_visual_instances(presentation::AgentDisplayBatch const& entities);
    void configure_ismc();
    void trigger_death_effects(std::span<sim::EntityId const> victims,
                               std::span<Vector3f const> death_locations);
    void draw_debugging_shapes(presentation::AgentDisplayBatch const& entities,
                               presentation::TargetView const& target_view) const;
    void validate_array_sizes(int32 count) const;

    FTurretConfig const* actor_config{nullptr};
    settings::TurretPresentation const* settings_{};

    UInstancedStaticMeshComponent* instances{nullptr};
    TArray<FTransform> ismc_transforms;

    FDrawDebugConfig debug_drawer;
    bool draw_target_arrows_enabled{false};
    bool draw_debug_entity_info_enabled{false};
};

} // namespace ioj
