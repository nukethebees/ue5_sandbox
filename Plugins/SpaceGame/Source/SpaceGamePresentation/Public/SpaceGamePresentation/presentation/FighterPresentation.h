#pragma once

#include <ioj/sim/fighters/sim.h>
#include <SpaceGamePresentation/entities/TeamColours.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>

#include <CoreMinimal.h>

class USandboxISMCComponent;

struct SPACEGAMEPRESENTATION_API FFighterPresentation {
    friend struct FLevelPresentation;
  public:
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FFighterPresentation(USandboxISMCComponent& component);

    void set_actor_config(FFighterConfig const* new_config) noexcept;
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

    USandboxISMCComponent* instances{nullptr};

    FFighterConfig const* actor_config{nullptr};
    FTeamColours team_colours_;
    TArray<int32> visible_indices_;

    FDrawDebugConfig debug_drawer;
    bool enable_target_debug_drawing{false};
    bool enable_ship_location_debug_drawing{false};

    ::ioj::sim::fighters::Sim const* simulation_{};
};
