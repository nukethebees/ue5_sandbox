#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/entity_registry_bookkeeping.h>
#include <ioj/sim/entity_registry_statistics.h>
#include <ioj/sim/spawned_entity_handles.h>

#include "ioj/sim/registry_entity_data.h"

#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_handle.h>
#include <ioj/sim/entity_history.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/health.h>
#include <ioj/sim/registry_entity_handles.h>

namespace ioj::sim {

struct EntityRegistry {
  public:
    using EntityData = RegistryEntityData;
    using TeamCounts = telemetry::TeamCounts;
    using EntityTypeCounts = telemetry::EntityTypeCounts;
    using EntityCounts = telemetry::EntityCounts;
    using Uint64EntityTypeCounts = telemetry::Uint64EntityTypeCounts;
    using Uint64EntityCounts = telemetry::Uint64EntityCounts;
    using DoubleEntityTypeCounts = telemetry::DoubleEntityTypeCounts;
    using DoubleEntityCounts = telemetry::DoubleEntityCounts;
    using KillMatrix = telemetry::KillMatrix;
    using CombatTelemetryCounters = telemetry::CombatTelemetryCounters;

    struct ConstView {
        auto get_num() const { return indices.size(); }

        std::span<RegistryEntityHandle const> indices;
        EntityData::ConstView data;
    };
    struct View {
        auto get_num() const { return indices.size(); }

        std::span<RegistryEntityHandle const> indices;
        EntityData::View data;
    };

    static constexpr std::uint8_t TEAM_COUNT{static_cast<std::uint8_t>(Team::COUNT)};

    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    // Starts a new identity lifetime; callers must discard pre-reset handles and IDs.
    void reset();
    // Clears the movement list published by the previous tick.
    void begin_tick();
    // Applies and consumes one queued final row per entity, then deaths.
    void commit_updates();
    // Publishes dead slots for reuse and clears transient damage and death observations.
    void end_tick();

    /* **************************************** */
    // Entity creation
    /* **************************************** */
    // Reused slots increment generation once; newly appended slots start at generation zero.
    auto add_entities(EntityData::ConstView const view) -> SpawnedEntityHandles;

    /* **************************************** */
    // Queued updates
    /* **************************************** */
    // Each handle may occur once per commit. Entity type is spawn-only.
    // Health/team changes also update history and alive counts.
    void queue_entity_updates(ConstView const view, EntityDeathInfo const& death_info);

    /* **************************************** */
    // Damage events
    /* **************************************** */
    void queue_direct_damage_events(DirectDamageEventsConstView damage_events);
    void queue_direct_damage_events(DirectDamageEvents const& damage_events) {
        queue_direct_damage_events(damage_events.get_const_view());
    }
    void record_shots(std::span<RegistryEntityHandle const> instigators);
    auto get_direct_damage_queue_view() const -> DirectDamageEvents const&;

    /* **************************************** */
    // Handle queries
    /* **************************************** */
    // Active means the slot generation matches, including a current dead occupant.
    auto analyse_handle(RegistryEntityHandle const handle) const -> RegistryHandleState;
    auto is_valid_handle(RegistryEntityHandle const handle) const -> bool;
    auto is_valid_alive(RegistryEntityHandle const handle) const -> bool;
    auto is_valid_dead(RegistryEntityHandle const handle) const -> bool;
    auto is_stale(RegistryEntityHandle const handle) const -> bool;

    /* **************************************** */
    // Entity data updates
    /* **************************************** */
    // Clears stale/dead handles; null stays null and invalid handles assert.
    void refresh_handles(std::span<RegistryEntityHandle> handles) const;
    void refresh_locations(std::span<RegistryEntityHandle const> handles,
                           Vectors3fView const& locations);
    // Empty views are considered to be unused parameters
    void refresh_entity_data(std::span<RegistryEntityHandle> handles,
                             Vectors3fView const& locations,
                             Vectors3fView const& velocities);

    /* **************************************** */
    // Entity data queries
    /* **************************************** */
    auto get_entity_data() const noexcept -> EntityData const& { return entity_data; }
    auto get_generations() const noexcept -> std::span<int const> {
        return {bookkeeping_.generations.data(), bookkeeping_.generations.size()};
    }
    auto get_location(RegistryEntityHandle const handle) const -> Vector3f;
    auto get_velocity(RegistryEntityHandle const handle) const -> Vector3f;
    auto get_health(RegistryEntityHandle const handle) const -> Health;
    auto get_team(RegistryEntityHandle const handle) const -> Team;
    auto get_entity_type(RegistryEntityHandle const handle) const -> EntityType;
    auto get_alive(RegistryEntityHandle const handle) const -> bool;

    /* **************************************** */
    // Entity collection queries
    /* **************************************** */
    // First-change order; populated by commit_updates() and cleared by begin_tick().
    auto get_moved_entities_this_tick() const -> std::span<RegistryEntityHandle const>;
    auto get_dead_entities_this_frame() const -> std::span<RegistryEntityHandle const>;
    auto get_handles_not_in_team(Team const team) const -> std::vector<RegistryEntityHandle>;
    void get_handles_not_in_team(Team const team, std::vector<RegistryEntityHandle>& out) const;

    /* **************************************** */
    // Aggregate queries
    /* **************************************** */
    auto get_num_elements() const noexcept -> std::int32_t;
    auto get_num_alive_active_entities() const noexcept -> std::int32_t;
    auto count_kills() const noexcept -> std::int32_t;
    auto count_alive() const noexcept -> std::int32_t;
    auto count_alive(EntityType type) const noexcept -> std::int32_t;
    auto count_alive_per_team() const noexcept -> TeamCounts;
    auto count_alive_per_team_and_type() const noexcept -> EntityCounts;
    auto count_alive_not_on_team(Team const team) const noexcept -> std::int32_t;
    auto get_combat_telemetry() const noexcept -> CombatTelemetryCounters const& {
        return statistics_.combat_telemetry();
    }

    /* **************************************** */
    // Unique entity queries
    /* **************************************** */
    auto get_unique_entities() const noexcept -> EntityHistoryColumnsConstView {
        return unique_entity_history_.get_const_view().columns();
    }
    // Slot-to-ID mapping includes current dead occupants until their slots are reused.
    auto get_active_unique_ids() const noexcept -> std::span<EntityUniqueId const> {
        return {bookkeeping_.unique_ids.data(), bookkeeping_.unique_ids.size()};
    }
    auto is_valid_unique_id(EntityUniqueId const id) const -> bool;
    auto get_num_unique_ids_issued() const -> std::int32_t { return unique_entity_history_.num(); }
    auto find_unique_id(RegistryEntityHandle const handle) const -> EntityUniqueId;
    auto get_kills(EntityUniqueId const id) const -> std::uint32_t;

    /* **************************************** */
    // Spatial queries
    /* **************************************** */
    auto collect_entities_in_range(Vector3f const& origin,
                                   float const radius,
                                   std::span<RegistryEntityHandle> const out_entities) const
        -> std::int32_t;

    /* **************************************** */
    // Validation
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_handles(std::span<RegistryEntityHandle const> const handles);
  private:
    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    void refresh_free_indices();

    /* **************************************** */
    // Queued updates
    /* **************************************** */
    void commit_entity_updates();
    // Death events annotate history; health transitions own alive-count changes.
    void commit_death_updates();

    /* **************************************** */
    // Validation
    /* **************************************** */
    void validate_unique_queued_entity_update_handles() const;
    void validate_unique_ids() const;
    void validate_unique_entity_data() const;

    // Current slot data shares indices with bookkeeping generations and IDs. Reuse changes both.
    EntityData entity_data;
    EntityRegistryBookkeeping bookkeeping_;

    // Append-only rows indexed by unique ID until reset; handle/type stay fixed, while team and
    // life state track committed state. Old rows and their death/kill accounting survive slot
    // reuse.
    EntityHistory unique_entity_history_;

    // Queued updates
    EntityData queued_entity_data;
    EntityDeathInfo queued_death_infos;

    // Queued damage events
    DirectDamageEvents queued_direct_damage_events;

    EntityRegistryStatistics statistics_;
};

inline auto EntityRegistry::is_valid_handle(RegistryEntityHandle const handle) const -> bool {
    return bookkeeping_.is_valid_handle(handle);
}
inline auto EntityRegistry::is_valid_alive(RegistryEntityHandle const handle) const -> bool {
    return is_valid_handle(handle) && sim::is_alive(entity_data.healths[handle.index]);
}
inline auto EntityRegistry::is_valid_dead(RegistryEntityHandle const handle) const -> bool {
    return is_valid_handle(handle) && sim::is_dead(entity_data.healths[handle.index]);
}
} // namespace ioj::sim
