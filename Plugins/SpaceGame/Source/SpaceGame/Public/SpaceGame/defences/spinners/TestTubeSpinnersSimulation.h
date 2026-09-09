#pragma once

#include <SpaceGame/simulation/LevelSimulationConfig.h>

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersSoA.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>

#include <CoreMinimal.h>

class ATestBatchOrchestrator;
struct FLevelSimulation;
struct FSpinnerPresentation;
struct FSpinnerSimulationConfig;
struct FTestEntityRegistry;

namespace ml::test_tube_spinners {
class PhaseInterface;

struct SPACEGAME_API Simulation {
    using EntityData = ml::test_tube_spinners::EntityData;

    Simulation(FSimulationClock const& clock,
               FTestEntityRegistry& entity_registry,
               ml::test_lasers::Simulation& laser_simulation) noexcept;
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    void set_config(FSpinnerSimulationConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> int32;
    auto get_entity_registry() const -> FTestEntityRegistry const& { return entity_registry; }
    auto get_laser_simulation() const -> ml::test_lasers::Simulation const& {
        return laser_simulation;
    }

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;

    float entity_radius{0.f};
  private:
    /* **************************************** */
    // Simulation phases
    /* **************************************** */
    void begin_play();
    void update_timers(float dt);
    void move(float dt);
    void queue_commands();
    void end_tick();

    /* **************************************** */
    // Spawning
    /* **************************************** */
    void spawn_instances(FVectors3f::ConstView new_locations,
                         TConstArrayView<float> new_yaws,
                         TConstArrayView<int32> new_fire_point_indices);

    /* **************************************** */
    // Movement
    /* **************************************** */
    void rotate_instances(float dt);

    /* **************************************** */
    // Firing
    /* **************************************** */
    void fire_lasers();

    friend class PhaseInterface;
    friend class ::ATestBatchOrchestrator;
    friend struct ::FLevelSimulation;
    friend struct ::FSpinnerPresentation;
#if WITH_DEV_AUTOMATION_TESTS
    friend struct FSpinnerSpawnTestAccess;
#endif

    FSpinnerSimulationConfig config{};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTestEntityRegistry& entity_registry;
    ml::test_lasers::Simulation& laser_simulation;
    EntityData entities{};
    TArray<int32> indices_ready_to_fire;
    ml::test_lasers::SpawnRequests new_lasers;
};
} // namespace ml::test_tube_spinners
