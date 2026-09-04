#pragma once

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersSoA.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>

#include <CoreMinimal.h>

class ATestBatchOrchestrator;
class ATestTubeSpinners;
struct FTubeSpinnerConfig;
struct FTestEntityRegistry;

namespace ml::test_tube_spinners {
class PhaseInterface;

struct SPACEGAME_API Simulation {
    using EntityData = ml::test_tube_spinners::EntityData;

    void set_config(FTubeSpinnerConfig const& new_config) noexcept;
    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    void set_entity_registry(FTestEntityRegistry& new_registry) noexcept;
    void set_laser_simulation(ml::test_lasers::Simulation& new_simulation) noexcept;

    auto get_num_instances() const noexcept -> int32;
    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    auto get_laser_simulation() const -> ml::test_lasers::Simulation const* {
        return laser_simulation;
    }
    void validate_array_sizes() const;

    float entity_radius{0.f};
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void update_timers(float dt);
    void move(float dt);
    void queue_commands();
    void update_entity_registry();
    void end_tick();

    void spawn_instances(FVectors3f::ConstView new_locations,
                         TConstArrayView<float> new_yaws,
                         TConstArrayView<int32> new_fire_point_indices);
    void rotate_instances(float dt);
    void fire_lasers();

    friend class PhaseInterface;
    friend class ::ATestTubeSpinners;

    FTubeSpinnerConfig const* config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTestEntityRegistry* entity_registry{nullptr};
    ml::test_lasers::Simulation* laser_simulation{nullptr};
    EntityData entities{};
    TArray<int32> indices_ready_to_fire;
    ml::test_lasers::SpawnRequests new_lasers;
};
} // namespace ml::test_tube_spinners
