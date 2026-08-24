#pragma once

#include <SandboxCore/time_series_data.h>

#include <HAL/Platform.h>

struct FTestEntityRegistry;

class SANDBOX_API FLevelTelemetryManager {
  public:
    using tick_type = uint64;
    using ActiveEntityCountData = ml::XYSeriesData<tick_type, int32>;

    void initialise(FTestEntityRegistry const& entity_registry);
    void reset();
    void tick(tick_type tick, FTestEntityRegistry const& entity_registry);

    auto get_active_entity_count_data() const noexcept -> ActiveEntityCountData const& {
        return active_entity_count_data_;
    }
  private:
    void sample_active_entity_count(tick_type tick, FTestEntityRegistry const& entity_registry);

    ActiveEntityCountData active_entity_count_data_;
};
