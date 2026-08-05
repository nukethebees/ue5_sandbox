#pragma once

#include <CoreMinimal.h>

class ATestBatchOrchestrator;

namespace ml::test_batch_orchestrator {
class SANDBOX_API SimulationClockInterface {
  public:
    using tick_type = uint64;
    using time_type = double;

    void bind(ATestBatchOrchestrator const& new_orchestrator) noexcept;

    auto frequency_to_tick_period(time_type const frequency) const noexcept -> tick_type;

    auto duration_to_tick_period(time_type const duration) const noexcept -> tick_type;

    auto get_completed_ticks() const noexcept -> tick_type;
  private:
    ATestBatchOrchestrator const* orchestrator{nullptr};
};
}
