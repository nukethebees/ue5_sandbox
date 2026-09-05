#pragma once

#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/DrawDebugConfig.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <CoreMinimal.h>

struct FDelayedNiagaraSpawns;
class UInstancedStaticMeshComponent;

struct SPACEGAME_API FCapitalPresentation {
    friend struct FLevelPresentation;
    friend struct FLevelSimulation;
  public:
    static constexpr bool is_world_space{false};
    static constexpr int32 n_custom_ismc_floats{3};

    explicit FCapitalPresentation(UInstancedStaticMeshComponent& component);

    void set_actor_config(FCapitalShipConfig const* new_config) noexcept;
  private:
    void bind_simulation(ml::test_capital_ships::Simulation& new_simulation);
    auto simulation() -> ml::test_capital_ships::Simulation&;
    auto simulation() const -> ml::test_capital_ships::Simulation const&;
    void set_niagara_spawner(FDelayedNiagaraSpawns& spawner);

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void commit_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void add_initial_visual_instances();
    void add_visual_instances(int32 first_index, int32 count);
    void trigger_death_effects();
    void draw_debugging_shapes() const;
    void visual_log_state() const;
    void validate_array_sizes() const;

    FCapitalShipConfig const* actor_config{nullptr};

    UInstancedStaticMeshComponent* instances{nullptr};
    FDelayedNiagaraSpawns* niagara_spawner{nullptr};

    FDrawDebugConfig debug_drawer;
    bool debugging_shapes_enabled{false};

    ml::test_capital_ships::Simulation* bound_simulation{nullptr};
};
