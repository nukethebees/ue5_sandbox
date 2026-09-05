#pragma once

#include <SpaceGame/combat/lasers/TestLasersPhaseInterface.h>
#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersPhaseInterface.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersSimulation.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsPhaseInterface.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/levels/LevelEventManager.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/ships/capital/TestCapitalShipsPhaseInterface.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersPhaseInterface.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/ships/player/TestSpaceShipPhaseInterface.h>
#include <SpaceGame/ships/player/TestSpaceShipSimulation.h>
#include <SpaceGame/simulation/LevelTelemetryManager.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/support/FixedTickLoop.h>

#include <SpaceGame/presentation/LevelPresentation.h>
#include <SpaceGame/simulation/LevelSimulationState.h>

struct FLevelSimulationInitData {
    static constexpr int32 player_target_spawn_index{-2};

    FFixedTickLoop clock_settings{};
    FLaserSimulationConfig lasers;
    FCapitalSimulationConfig capital_ships;
    FFighterSimulationConfig fighters;
    FTurretSimulationConfig turrets;
    FSpinnerSimulationConfig spinners;
    TOptional<ml::test_space_ship::FPlayerSpawnData> player;
    ml::test_capital_ships::SpawnData capital_spawns;
    TArray<int32> capital_target_spawn_indices;
    ml::test_static_turrets::SpawnData turret_spawns;
    ml::FCompiledLevelEvents level_events;
    FVectors3f spinner_locations;
    TArray<float> spinner_yaws;
    TArray<int32> spinner_fire_points;
    TArray<FTransform> turret_transforms;
    ml::ioj::FEntityAABBs entity_bounds{};
    ml::WorldAABBs static_bounds{};
    FIntVector3 grid_dimensions{400, 400, 5};
    FVector3f cell_size{5000.f, 5000.f, 20000.f};
    float capital_radius{1.f};
    float fighter_radius{1.f};
    float turret_radius{1.f};
    float spinner_radius{1.f};
    float fighter_fire_point_distance{};
};

struct SPACEGAME_API FLevelSimulation {
    using tick_type = FSimulationClock::tick_type;
    using time_type = FSimulationClock::time_type;

    explicit FLevelSimulation(FLevelSimulationInitData data,
                              FLevelPresentationResources const* presentation = nullptr);
    FLevelSimulation(FLevelSimulation const&) = delete;
    FLevelSimulation(FLevelSimulation&&) = delete;
    auto operator=(FLevelSimulation const&) -> FLevelSimulation& = delete;
    auto operator=(FLevelSimulation&&) -> FLevelSimulation& = delete;

    void finish_initialisation();
    void start();
    void pause();
    void advance(time_type dt);
    void commit_presentation(time_type dt);
    void set_time_scale(time_type scale);
    auto get_state() const noexcept -> EOrchestratorState { return state_; }
    auto get_clock() const noexcept -> FSimulationClock const& { return clock_; }
    auto has_presentation() const noexcept -> bool { return presentation_.IsSet(); }
    auto get_player_ship_simulation() -> ml::test_space_ship::Simulation* {
        return player_ship_simulation_.IsSet() ? &player_ship_simulation_.GetValue() : nullptr;
    }
    auto get_player_ship_simulation() const -> ml::test_space_ship::Simulation const* {
        return player_ship_simulation_.IsSet() ? &player_ship_simulation_.GetValue() : nullptr;
    }
    auto get_lasers() -> ml::test_lasers::Simulation* { return &lasers_simulation_; }
    auto get_lasers() const -> ml::test_lasers::Simulation const* { return &lasers_simulation_; }
    auto get_capital_ships() -> ml::test_capital_ships::Simulation* {
        return &capital_ships_simulation_;
    }
    auto get_capital_ships() const -> ml::test_capital_ships::Simulation const* {
        return &capital_ships_simulation_;
    }
    auto get_capital_ship_fighters() -> ml::test_capital_ship_fighters::Simulation* {
        return &capital_ship_fighters_simulation_;
    }
    auto get_capital_ship_fighters() const -> ml::test_capital_ship_fighters::Simulation const* {
        return &capital_ship_fighters_simulation_;
    }
    auto get_turrets() -> ml::test_static_turrets::Simulation* { return &turrets_simulation_; }
    auto get_turrets() const -> ml::test_static_turrets::Simulation const* {
        return &turrets_simulation_;
    }
    auto get_spinners() -> ml::test_tube_spinners::Simulation* { return &spinners_simulation_; }
    auto get_spinners() const -> ml::test_tube_spinners::Simulation const* {
        return &spinners_simulation_;
    }
    auto get_entity_registry() -> FTestEntityRegistry& { return entity_registry_; }
    auto get_entity_registry() const -> FTestEntityRegistry const& { return entity_registry_; }
    auto get_mission_manager() -> FTestMissionManager& { return mission_manager_; }
    auto get_mission_manager() const -> FTestMissionManager const& { return mission_manager_; }
    auto get_spatial_query_manager() -> ml::FSpatialQueryManager& { return query_manager_; }
    auto get_spatial_query_manager() const -> ml::FSpatialQueryManager const& {
        return query_manager_;
    }
    auto get_level_telemetry_manager() -> FLevelTelemetryManager& {
        return level_telemetry_manager_;
    }
    auto get_level_telemetry_manager() const -> FLevelTelemetryManager const& {
        return level_telemetry_manager_;
    }

    TFunction<void()> on_mission_evaluated;
    TFunction<void(FLevelSimulation&)> on_end_tick;
  private:
    friend class ATestBatchOrchestrator;
    void bind_simulation_dependencies();

    FSimulationClock clock_;
    EOrchestratorState state_{EOrchestratorState::Uninitialised};
    FTestEntityRegistry entity_registry_;
    FTestMissionManager mission_manager_;
    ml::FSpatialQueryManager query_manager_;
    ml::FLevelEventManager event_manager_;
    FLevelTelemetryManager level_telemetry_manager_;
    TOptional<ml::test_space_ship::Simulation> player_ship_simulation_;
    ml::test_space_ship::PhaseInterface player_ship_phase_;
    ml::test_lasers::Simulation lasers_simulation_;
    ml::test_lasers::PhaseInterface lasers_phase_;
    ml::test_capital_ships::Simulation capital_ships_simulation_;
    ml::test_capital_ships::PhaseInterface capital_ships_phase_;
    ml::test_capital_ship_fighters::Simulation capital_ship_fighters_simulation_;
    ml::test_capital_ship_fighters::PhaseInterface capital_ship_fighters_phase_;
    ml::test_static_turrets::Simulation turrets_simulation_;
    ml::test_static_turrets::PhaseInterface turrets_phase_;
    ml::test_tube_spinners::Simulation spinners_simulation_;
    ml::test_tube_spinners::PhaseInterface spinners_phase_;
    TOptional<FLevelPresentation> presentation_;
};
