#pragma once
#include <ioj/presentation/mission_frame.h>
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
    void begin_play_presentation(presentation::MissionFrame const& frame);
    void update_visual_data(presentation::FrameEvents const& events);
    void commit_visual_data();
    void end_tick_presentation(presentation::MissionFrame const& frame);

    void configure_ismc();
    void add_initial_visual_instances(presentation::MissionFrame const& frame);
    void add_visual_instances(presentation::MissionFrame const& frame,
                              int32 first_index,
                              int32 count);
    void trigger_death_effects(presentation::FrameEvents const& events);
    void validate_array_sizes(presentation::MissionFrame const& frame) const;

    FCapitalShipConfig const* actor_config{nullptr};
    settings::CapitalPresentation const* settings_{};

    UInstancedStaticMeshComponent* instances{nullptr};
    FDelayedNiagaraSpawns* niagara_spawner{nullptr};
};

} // namespace ioj
