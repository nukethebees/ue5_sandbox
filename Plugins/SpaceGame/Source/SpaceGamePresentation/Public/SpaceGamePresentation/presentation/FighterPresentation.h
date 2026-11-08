#pragma once

#include <ioj/sim/fighters/sim.h>
#include <SpaceGamePresentation/entities/TeamColours.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>

#include <CoreMinimal.h>

namespace ioj {
class USandboxISMCComponent;
struct FLevelPresentation;
} // namespace ioj

struct SPACEGAMEPRESENTATION_API FFighterPresentation {
    friend struct ioj::FLevelPresentation;
  public:
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FFighterPresentation(ioj::USandboxISMCComponent& component);

    void set_actor_config(ioj::FFighterConfig const* new_config) noexcept;
  private:
    auto simulation() const -> ::ioj::sim::fighters::Sim const& { return *simulation_; }

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void update_ismc();
    void draw_debug_shapes();
    void validate_array_sizes() const;

    ioj::USandboxISMCComponent* instances{nullptr};

    ioj::FFighterConfig const* actor_config{nullptr};
    FTeamColours team_colours_;
    TArray<int32> visible_indices_;

    ioj::FDrawDebugConfig debug_drawer;
    bool enable_target_debug_drawing{false};
    bool enable_ship_location_debug_drawing{false};

    ::ioj::sim::fighters::Sim const* simulation_{};
};
