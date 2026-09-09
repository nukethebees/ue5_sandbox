#pragma once

#include <CoreMinimal.h>

#include <SpaceGame/simulation/SimulationClock.h>

namespace ml::test_batch_orchestrator {
class SPACEGAME_API SimulationClockInterface {
  public:
    using tick_type = uint64;
    using time_type = double;

    SimulationClockInterface(FSimulationClock const& orch);

    SimulationClockInterface(SimulationClockInterface const&) = delete;
    SimulationClockInterface& operator=(SimulationClockInterface const&) = delete;
    SimulationClockInterface(SimulationClockInterface&&) = delete;
    SimulationClockInterface& operator=(SimulationClockInterface&&) = delete;

    auto frequency_to_tick_period(time_type const frequency) const noexcept -> tick_type;

    auto duration_to_tick_period(time_type const duration) const noexcept -> tick_type;

    auto get_completed_ticks() const noexcept -> tick_type;
    auto get_simulation_time() const noexcept -> time_type;
    auto get_tick_period() const noexcept -> time_type;
  private:
    FSimulationClock const& orchestrator;
};
}
