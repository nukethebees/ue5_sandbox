#pragma once

#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/DrawDebugConfig.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

struct SPACEGAME_API FFighterPresentation {
    friend struct FLevelPresentation;
    friend struct FLevelSimulation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FFighterPresentation(UInstancedStaticMeshComponent& component);

    void set_actor_config(FFighterConfig const* new_config) noexcept;
  private:
    void bind_simulation(ml::test_capital_ship_fighters::Simulation& new_simulation);
    auto simulation() -> ml::test_capital_ship_fighters::Simulation&;
    auto simulation() const -> ml::test_capital_ship_fighters::Simulation const&;

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
    void visual_log_state() const;
    void validate_array_sizes() const;

    UInstancedStaticMeshComponent* instances{nullptr};

    FFighterConfig const* actor_config{nullptr};
    TArray<FTransform> ismc_transforms;
    TArray<FTransform> dummy_transforms_spawn_buffer;
    TArray<float> custom_data_buffer;

    FDrawDebugConfig debug_drawer;
    bool enable_target_debug_drawing{false};
    bool enable_ship_location_debug_drawing{false};

    ml::test_capital_ship_fighters::Simulation* bound_simulation{nullptr};
};
