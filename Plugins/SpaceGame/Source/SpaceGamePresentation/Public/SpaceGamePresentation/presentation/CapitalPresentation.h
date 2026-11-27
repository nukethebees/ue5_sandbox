#pragma once
#include <ioj/presentation/level_frame.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

struct FDelayedNiagaraSpawns;
class UInstancedStaticMeshComponent;

namespace ioj {
struct FLevelPresentation;
} // namespace ioj

namespace ioj {
struct SPACEGAMEPRESENTATION_API FCapitalPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FCapitalPresentation(UInstancedStaticMeshComponent& component);

    void set_actor_config(FCapitalShipConfig const* new_config,
                          settings::CapitalPresentation const& settings) noexcept;
  private:
    void set_niagara_spawner(FDelayedNiagaraSpawns& spawner);
    void ValidateOptionalAssets() const;

    void clear_runtime_state_presentation();
    void begin_play_presentation(presentation::AgentDisplayBatch const& entities,
                                 presentation::rotators3f::ConstView rotations);
    void update_visual_data(presentation::InstanceChanges const& changes);
    void commit_visual_data();

    void configure_ismc();
    void add_initial_visual_instances(presentation::AgentDisplayBatch const& entities,
                                      presentation::rotators3f::ConstView rotations);
    void trigger_death_effects(std::span<sim::FrameEventCounts const> ticks,
                               std::span<sim::EntityId const> victims,
                               std::span<Vector3f const> death_locations);
    void validate_array_sizes(int32 count) const;

    FCapitalShipConfig const* actor_config{nullptr};
    settings::CapitalPresentation const* settings_{};

    UInstancedStaticMeshComponent* instances{nullptr};
    FDelayedNiagaraSpawns* niagara_spawner{nullptr};
};

} // namespace ioj
