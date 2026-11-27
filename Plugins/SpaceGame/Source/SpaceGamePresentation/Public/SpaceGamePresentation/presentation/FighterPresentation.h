#pragma once

#include <ioj/presentation/level_frame.h>
#include <SpaceGamePresentation/entities/TeamColours.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>

#include <CoreMinimal.h>

namespace ioj {
class USandboxISMCComponent;
struct FLevelPresentation;
} // namespace ioj

namespace ioj {
struct SPACEGAMEPRESENTATION_API FFighterPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FFighterPresentation(USandboxISMCComponent& component);

    void set_actor_config(FFighterConfig const* new_config) noexcept;
  private:
    void clear_runtime_state_presentation();
    void begin_play_presentation(presentation::AgentDisplayBatch const& entities,
                                 presentation::vectors3f::ConstView aim_directions,
                                 presentation::TargetView const& target_view);
    void update_visual_data(presentation::AgentDisplayBatch const& entities,
                            presentation::vectors3f::ConstView aim_directions);
    void commit_visual_data(presentation::vectors3f::ConstView locations,
                            presentation::TargetView const& target_view);

    void configure_ismc();
    void update_ismc(presentation::AgentDisplayBatch const& entities,
                     presentation::vectors3f::ConstView aim_directions);
    void draw_debug_shapes(presentation::vectors3f::ConstView locations,
                           presentation::TargetView const& target_view);

    USandboxISMCComponent* instances{nullptr};

    FFighterConfig const* actor_config{nullptr};
    FTeamColours team_colours_;
    TArray<int32> visible_indices_;

    FDrawDebugConfig debug_drawer;
    bool enable_target_debug_drawing{false};
    bool enable_ship_location_debug_drawing{false};
};

} // namespace ioj
