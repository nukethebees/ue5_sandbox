#pragma once

#include <ioj/sim/system_read_views.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

class UInstancedStaticMeshComponent;

struct SPACEGAMEPRESENTATION_API FSpinnerPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr bool is_world_space{false};

    explicit FSpinnerPresentation(UInstancedStaticMeshComponent& component);
    void set_actor_config(FTubeSpinnerConfig const* new_config) noexcept;
  private:
    auto view() const -> ::ioj::sim::SpinnerReadView const& { return view_; }

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void update_ismc_transforms();
    void update_ismc();
    void validate_array_sizes() const;

    FTubeSpinnerConfig const* actor_config{nullptr};
    ::ioj::sim::SpinnerReadView view_{};

    UInstancedStaticMeshComponent* instances{nullptr};
    TArray<FTransform> ismc_transforms;
};
