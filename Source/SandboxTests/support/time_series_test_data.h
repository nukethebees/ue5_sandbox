#pragma once

#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCore/time_series_data.h>

#include <limits>

namespace ml {
template <typename... ValueTypes>
void reset_and_reserve_time_series(ATestBatchOrchestrator const& orchestrator,
                                   ATestBatchOrchestrator::time_type const duration,
                                   TimeSeriesData<ValueTypes>&... data) {
    auto const tick_count{orchestrator.duration_to_tick_period(duration)};
    check(tick_count < static_cast<uint64>(std::numeric_limits<int32>::max()));

    auto const sample_count{static_cast<int32>(tick_count) + 1};
    (data.reset(), ...);
    (data.reserve(sample_count), ...);
}
}
