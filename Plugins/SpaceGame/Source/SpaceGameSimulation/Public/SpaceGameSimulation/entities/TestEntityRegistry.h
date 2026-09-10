#pragma once

#include "SpaceGameSimulation/entities/TestEntityRegistryData.h"

#include <SandboxCoreEngine/enums.h>
#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/RegistryEntityHandles.h>
#include <SpaceGameSimulation/entities/RegistryHandleState.h>
#include <SpaceGameSimulation/entities/TestEntityUniqueEntityData.h>
#include <SpaceGameSimulation/entities/TestEntityUniqueId.h>
#include <SpaceGameSimulation/entities/TestTeam.h>

#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

struct SpawnedEntityHandles {
    FRegistryEntityHandles registry_handles;
    // Handles retain input order; the ID at input index i is first_id + i.
    TestEntityUniqueId first_id;

    auto num() const -> int32;
    void reset();
    void add_defaulted(int32 const count);
    void add_uninitialised(int32 const count);
};

struct SPACEGAMESIMULATION_API FTestEntityRegistry {
  public:
    using EntityData = ml::entity_registry::EntityData;
    using TeamCounts = TStaticArray<int32, ml::EnumCountTrait<ETestTeam>::count_value>;
    using EntityTypeCounts = TStaticArray<int32, ml::EnumCountTrait<ETestEntityType>::count_value>;
    using EntityCounts = TStaticArray<EntityTypeCounts, ml::EnumCountTrait<ETestTeam>::count_value>;
    using Uint64EntityTypeCounts =
        TStaticArray<uint64, ml::EnumCountTrait<ETestEntityType>::count_value>;
    using Uint64EntityCounts =
        TStaticArray<Uint64EntityTypeCounts, ml::EnumCountTrait<ETestTeam>::count_value>;
    using DoubleEntityTypeCounts =
        TStaticArray<double, ml::EnumCountTrait<ETestEntityType>::count_value>;
    using DoubleEntityCounts =
        TStaticArray<DoubleEntityTypeCounts, ml::EnumCountTrait<ETestTeam>::count_value>;
    using KillMatrix =
        TStaticArray<TStaticArray<uint64, ml::EnumCountTrait<ETestTeam>::count_value>,
                     ml::EnumCountTrait<ETestTeam>::count_value>;

    struct CombatTelemetryCounters {
        Uint64EntityCounts spawned{};
        Uint64EntityCounts destroyed{};
        Uint64EntityCounts shots{};
        Uint64EntityCounts hits{};
        DoubleEntityCounts damage_dealt{};
        DoubleEntityCounts damage_received{};
        Uint64EntityCounts kills{};
        Uint64EntityCounts losses{};
        KillMatrix kill_matrix{};
    };

    struct ConstView {
        auto get_num() const { return indices.Num(); }

        TConstArrayView<FRegistryEntityHandle> indices;
        EntityData::ConstView data;
    };
    struct View {
        auto get_num() const { return indices.Num(); }

        TConstArrayView<FRegistryEntityHandle> indices;
        EntityData::View data;
    };

    static constexpr uint8 TEAM_COUNT{static_cast<uint8>(ETestTeam::COUNT)};

    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    // Starts a new identity lifetime; callers must discard pre-reset handles and IDs.
    void reset();
    // Clears the movement list published by the previous tick.
    void begin_tick();
    // Applies one queued final row per entity, then deaths. Queues are retained until end_tick().
    void commit_updates();
    // Publishes dead slots for reuse and clears this tick's queues and death list.
    void end_tick();

    /* **************************************** */
    // Entity creation
    /* **************************************** */
    // Reused slots increment generation once; newly appended slots start at generation zero.
    auto add_entities(EntityData::ConstView const view) -> SpawnedEntityHandles;

    /* **************************************** */
    // Queued updates
    /* **************************************** */
    // Each handle may occur once between end_tick() calls. Radius and entity type are spawn-only.
    // Alive/team changes also update history and counts.
    void queue_entity_updates(ConstView const view, EntityDeathInfo const& death_info);

    /* **************************************** */
    // Damage events
    /* **************************************** */
    void queue_direct_damage_events(DirectDamageEvents const& damage_events);
    void record_shots(TConstArrayView<FRegistryEntityHandle> instigators);
    auto get_direct_damage_queue_view() const -> DirectDamageEvents const&;

    /* **************************************** */
    // Handle queries
    /* **************************************** */
    // Active means the slot generation matches, including a current dead occupant.
    auto analyse_handle(FRegistryEntityHandle const handle) const -> ERegistryHandleState;
    auto is_valid_handle(FRegistryEntityHandle const handle) const -> bool;
    auto is_valid_alive(FRegistryEntityHandle const handle) const -> bool;
    auto is_valid_dead(FRegistryEntityHandle const handle) const -> bool;
    auto is_stale(FRegistryEntityHandle const handle) const -> bool;

    /* **************************************** */
    // Entity data updates
    /* **************************************** */
    // Clears stale/dead handles; null stays null and invalid handles assert.
    void refresh_handles(TArrayView<FRegistryEntityHandle> handles) const;
    void refresh_locations(TConstArrayView<FRegistryEntityHandle> handles,
                           FVectors3f::View const& locations);
    // Empty views are considered to be unused parameters
    void refresh_entity_data(TArrayView<FRegistryEntityHandle> handles,
                             FVectors3f::View const& locations,
                             FVectors3f::View const& velocities,
                             TArrayView<float> radii);

    /* **************************************** */
    // Entity data queries
    /* **************************************** */
    auto get_entity_data() const noexcept -> EntityData const& { return entity_data; }
    auto get_generations() const noexcept -> TConstArrayView<int> { return generations; }
    auto get_location(FRegistryEntityHandle const handle) const -> FVector3f;
    auto get_velocity(FRegistryEntityHandle const handle) const -> FVector3f;
    auto get_health(FRegistryEntityHandle const handle) const -> int32;
    auto get_team(FRegistryEntityHandle const handle) const -> ETestTeam;
    auto get_entity_type(FRegistryEntityHandle const handle) const -> ETestEntityType;
    auto get_alive(FRegistryEntityHandle const handle) const -> bool;

    /* **************************************** */
    // Entity collection queries
    /* **************************************** */
    // First-change order; populated by commit_updates() and cleared by begin_tick().
    auto get_moved_entities_this_tick() const -> TConstArrayView<FRegistryEntityHandle>;
    auto get_dead_entities_this_frame() const -> TConstArrayView<FRegistryEntityHandle>;
    auto get_handles_not_in_team(ETestTeam const team) const -> TArray<FRegistryEntityHandle>;
    void get_handles_not_in_team(ETestTeam const team, TArray<FRegistryEntityHandle>& out) const;

    /* **************************************** */
    // Aggregate queries
    /* **************************************** */
    auto get_num_elements() const noexcept -> int32;
    auto get_num_alive_active_entities() const noexcept -> int32;
    auto count_kills() const noexcept -> int32;
    auto count_alive() const noexcept -> int32;
    auto count_alive(ETestEntityType type) const noexcept -> int32;
    auto count_alive_per_team() const noexcept -> TeamCounts;
    auto count_alive_per_team_and_type() const noexcept -> EntityCounts;
    auto count_alive_not_on_team(ETestTeam const team) const noexcept -> int32;
    auto get_combat_telemetry() const noexcept -> CombatTelemetryCounters const& {
        return combat_telemetry_;
    }

    /* **************************************** */
    // Unique entity queries
    /* **************************************** */
    auto get_unique_entities() const noexcept -> TestEntityUniqueEntityData const& {
        return unique_entities;
    }
    // Slot-to-ID mapping includes current dead occupants until their slots are reused.
    auto get_active_unique_ids() const noexcept -> TConstArrayView<TestEntityUniqueId> {
        return unique_ids;
    }
    auto is_valid_unique_id(TestEntityUniqueId const id) const -> bool;
    auto get_num_unique_ids_issued() const -> int32 { return ml::num(unique_entities); }
    auto find_unique_id(FRegistryEntityHandle const handle) const -> TestEntityUniqueId;
    auto get_kills(TestEntityUniqueId const id) const -> TestEntityUniqueEntityData::kills_type;

    /* **************************************** */
    // Spatial queries
    /* **************************************** */
    auto collect_entities_in_range(FVector3f const& origin,
                                   float const radius,
                                   TArrayView<FRegistryEntityHandle> const out_entities) const
        -> int32;

    /* **************************************** */
    // Validation
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_handles(TConstArrayView<FRegistryEntityHandle> const handles);
  private:
    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    void refresh_free_indices();

    /* **************************************** */
    // Slot allocation and identity registration
    /* **************************************** */
    auto register_spawned_entity(EntityData::ConstView const& view,
                                 int32 source_index,
                                 int32 slot_index,
                                 TestEntityUniqueId unique_id) -> FRegistryEntityHandle;

    /* **************************************** */
    // Live state and alive counts
    /* **************************************** */
    void adjust_alive_count(ETestTeam team, ETestEntityType type, int32 delta);
    void apply_live_state_transition(int32 slot_index, ETestTeam team, uint8 alive);

    /* **************************************** */
    // Queued updates
    /* **************************************** */
    void commit_entity_updates();
    // Death events annotate history; live-state transitions own alive-count changes.
    void commit_death_updates();
    void record_entity_death(TestEntityUniqueId victim_id, ETestDeathReason reason);
    void credit_entity_kill(TestEntityUniqueId killer_id, TestEntityUniqueId victim_id);

    /* **************************************** */
    // Validation
    /* **************************************** */
    void validate_unique_queued_entity_update_handles() const;
    void validate_unique_ids() const;
    void validate_unique_entity_data() const;

    // Current slot data: all three arrays share slot indices. Reuse changes the generation and ID.
    EntityData entity_data;
    TArray<int32> generations;
    TArray<TestEntityUniqueId> unique_ids;

    // Append-only rows indexed by unique ID until reset; handle/type stay fixed, team/alive track
    // committed state. Old rows and their death/kill accounting survive slot reuse.
    TestEntityUniqueEntityData unique_entities;

    // Queued updates
    EntityData queued_entity_data;
    TArray<FRegistryEntityHandle> queued_entity_update_handles;
    EntityDeathInfo queued_death_infos;

    // Queued damage events
    DirectDamageEvents queued_direct_damage_events;

    // Per-tick entity changes
    TArray<FRegistryEntityHandle> dead_entities_this_frame;
    TArray<FRegistryEntityHandle> moved_entities_this_tick_;
    // Ascending dead-slot snapshot from end_tick(), consumed from the tail by add_entities().
    TArray<int32> free_indices;

    EntityCounts alive_counts_{};
    int32 alive_count_{};
    int32 cumulative_kill_count_{};
    CombatTelemetryCounters combat_telemetry_{};
};

inline auto FTestEntityRegistry::is_valid_handle(FRegistryEntityHandle const handle) const -> bool {
    return generations.IsValidIndex(handle.index) &&
           (generations[handle.index] == handle.generation);
}
inline auto FTestEntityRegistry::is_valid_alive(FRegistryEntityHandle const handle) const -> bool {
    return is_valid_handle(handle) && (entity_data.alive[handle.index] > 0);
}
inline auto FTestEntityRegistry::is_valid_dead(FRegistryEntityHandle const handle) const -> bool {
    return is_valid_handle(handle) && (entity_data.alive[handle.index] == 0);
}
