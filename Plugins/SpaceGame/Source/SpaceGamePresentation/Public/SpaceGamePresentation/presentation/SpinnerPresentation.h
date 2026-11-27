#pragma once

#include <ioj/presentation/level_frame.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

class UInstancedStaticMeshComponent;

namespace ioj {
struct FLevelPresentation;
} // namespace ioj

namespace ioj {
struct SPACEGAMEPRESENTATION_API FSpinnerPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr bool is_world_space{false};

    explicit FSpinnerPresentation(UInstancedStaticMeshComponent& component);
    void set_actor_config(FTubeSpinnerConfig const* new_config) noexcept;
  private:
    void clear_runtime_state_presentation();
    void begin_play_presentation(presentation::LevelFrame const& frame);
    void update_visual_data(presentation::LevelFrame const& frame);
    void commit_visual_data();
    void end_tick_presentation(presentation::LevelFrame const& frame);

    void configure_ismc();
    void update_ismc_transforms(presentation::LevelFrame const& frame);
    void update_ismc(presentation::LevelFrame const& frame);
    void validate_array_sizes(presentation::LevelFrame const& frame) const;

    FTubeSpinnerConfig const* actor_config{nullptr};

    UInstancedStaticMeshComponent* instances{nullptr};
    TArray<FTransform> ismc_transforms;
};

} // namespace ioj
