#pragma once

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SandboxCore/time_series_data.h>

#include <HAL/Platform.h>

class SPACEGAME_API FLevelTelemetryManager {
  public:
    using tick_type = uint64;
    using ActiveEntityCountData = ml::XYSeriesData<tick_type, int32>;
    using ActiveEntityCountsData = ml::XYSeriesData<tick_type, FTestEntityRegistry::EntityCounts>;
    using CumulativeKillCountData = ml::XYSeriesData<tick_type, int32>;
    using RegistrySlotCountData = ml::XYSeriesData<tick_type, int32>;
    using IssuedUniqueIdCountData = ml::XYSeriesData<tick_type, int32>;

    void initialise(FTestEntityRegistry const& entity_registry);
    void reset();
    void tick(tick_type tick, FTestEntityRegistry const& entity_registry);

    auto get_active_entity_count_data() const noexcept -> ActiveEntityCountData const& {
        return active_entity_count_data_;
    }
    auto get_active_entity_counts_data() const noexcept -> ActiveEntityCountsData const& {
        return active_entity_counts_data_;
    }
    auto get_cumulative_kill_count_data() const noexcept -> CumulativeKillCountData const& {
        return cumulative_kill_count_data_;
    }
    auto get_registry_slot_count_data() const noexcept -> RegistrySlotCountData const& {
        return registry_slot_count_data_;
    }
    auto get_issued_unique_id_count_data() const noexcept -> IssuedUniqueIdCountData const& {
        return issued_unique_id_count_data_;
    }
  private:
    void sample_active_entity_counts(tick_type tick, FTestEntityRegistry const& entity_registry);
    void sample_cumulative_kill_count(tick_type tick, FTestEntityRegistry const& entity_registry);
    void sample_registry_slot_count(tick_type tick, FTestEntityRegistry const& entity_registry);
    void sample_issued_unique_id_count(tick_type tick, FTestEntityRegistry const& entity_registry);

    ActiveEntityCountData active_entity_count_data_;
    ActiveEntityCountsData active_entity_counts_data_;
    CumulativeKillCountData cumulative_kill_count_data_;
    RegistrySlotCountData registry_slot_count_data_;
    IssuedUniqueIdCountData issued_unique_id_count_data_;
};
