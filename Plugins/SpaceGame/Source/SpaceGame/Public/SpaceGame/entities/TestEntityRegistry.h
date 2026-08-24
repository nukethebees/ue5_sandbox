#pragma once

#include "SpaceGame/entities/TestEntityRegistryData.h"

#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/entities/RegistryEntityHandles.h>
#include <SpaceGame/entities/RegistryHandleState.h>
#include <SpaceGame/entities/TestEntityUniqueEntityData.h>
#include <SpaceGame/entities/TestEntityUniqueId.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SandboxGameShared/utilities/enums.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "GameFramework/Actor.h"

struct SpawnedEntityHandles {
    FRegistryEntityHandles registry_handles;
    TestEntityUniqueId first_id;

    auto num() const -> int32;
    void reset();
    void add_defaulted(int32 const count);
    void add_uninitialised(int32 const count);
};

struct SPACEGAME_API FTestEntityRegistry {
  public:
    using EntityData = ml::entity_registry::EntityData;
    using TeamCounts = TStaticArray<int32, ml::EnumCountTrait<ETestTeam>::count_value>;
    using EntityTypeCounts = TStaticArray<int32, ml::EnumCountTrait<ETestEntityType>::count_value>;
    using EntityCounts = TStaticArray<EntityTypeCounts, ml::EnumCountTrait<ETestTeam>::count_value>;

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

    // Lifecycle
    void reset();
    void commit_updates();
    void end_tick();

    // Entity creation
    auto add_entities(EntityData::ConstView const view) -> SpawnedEntityHandles;

    // Queued updates
    void queue_entity_updates(ConstView const view, EntityDeathInfo const& death_info);

    // Damage events
    void queue_direct_damage_events(DirectDamageEvents const& damage_events);
    auto get_direct_damage_queue_view() const -> DirectDamageEvents const&;

    // Handle queries
    auto analyse_handle(FRegistryEntityHandle const handle) const -> ERegistryHandleState;
    auto is_valid_handle(FRegistryEntityHandle const index) const -> bool;
    auto is_valid_alive(FRegistryEntityHandle const handle) const -> bool;
    auto is_valid_dead(FRegistryEntityHandle const handle) const -> bool;
    auto is_stale(FRegistryEntityHandle const index) const -> bool;

    // Entity data updates
    void refresh_handles(TArrayView<FRegistryEntityHandle> handles) const;
    void refresh_locations(TConstArrayView<FRegistryEntityHandle> handles,
                           FVectors3f::View const& locations);
    // Empty views are considered to be unused parameters
    void refresh_entity_data(TArrayView<FRegistryEntityHandle> handles,
                             FVectors3f::View const& locations,
                             FVectors3f::View const& velocities,
                             TArrayView<float> radii);

    // Entity data queries
    auto get_entity_data() const noexcept -> EntityData const& { return entity_data; }
    auto get_generations() const noexcept -> TConstArrayView<int> { return generations; }
    auto get_location(FRegistryEntityHandle const index) const -> FVector3f;
    auto get_velocity(FRegistryEntityHandle const index) const -> FVector3f;
    auto get_health(FRegistryEntityHandle const index) const -> int32;
    auto get_team(FRegistryEntityHandle const index) const -> ETestTeam;
    auto get_entity_type(FRegistryEntityHandle const index) const -> ETestEntityType;
    auto get_alive(FRegistryEntityHandle const index) const -> bool;

    // Entity collection queries
    auto get_dead_entities_this_frame() const -> TConstArrayView<FRegistryEntityHandle>;
    auto get_handles_not_in_team(ETestTeam const team) const -> TArray<FRegistryEntityHandle>;
    void get_handles_not_in_team(ETestTeam const team, TArray<FRegistryEntityHandle>& out) const;

    // Aggregate queries
    auto get_num_elements() const noexcept -> int32;
    auto get_num_alive_active_entities() const noexcept -> int32;
    auto count_kills() const noexcept -> int32;
    auto count_alive() const noexcept -> int32;
    auto count_alive(ETestEntityType type) const noexcept -> int32;
    auto count_alive_per_team() const noexcept -> TeamCounts;
    auto count_alive_per_team_and_type() const noexcept -> EntityCounts;
    auto count_alive_not_on_team(ETestTeam const team) const noexcept -> int32;

    // Unique entity queries
    auto get_unique_entities() const noexcept -> TestEntityUniqueEntityData const& {
        return unique_entities;
    }
    auto get_active_unique_ids() const noexcept -> TConstArrayView<TestEntityUniqueId> {
        return unique_ids;
    }
    auto is_valid_unique_id(TestEntityUniqueId const id) const -> bool;
    auto get_num_unique_ids_issued() const -> int32 { return ml::num(unique_entities); }
    auto find_unique_id(FRegistryEntityHandle const handle) const -> TestEntityUniqueId;
    auto get_kills(TestEntityUniqueId const id) const -> TestEntityUniqueEntityData::kills_type;

    // Spatial queries
    auto collect_entities_in_range(FVector3f const& origin,
                                   float const radius,
                                   TArrayView<FRegistryEntityHandle> const out_entities) const
        -> int32;
    // Validation
    void validate_array_sizes() const;
    void validate_handles(TConstArrayView<FRegistryEntityHandle> const handles);
  private:
    // Lifecycle
    void refresh_free_indices();

    // Queued updates
    void commit_entity_updates();
    void commit_death_updates();

    // Validation
    void validate_unique_ids() const;
    void validate_unique_entity_data() const;

    // Entity data
    EntityData entity_data;
    TArray<int32> generations;
    TArray<TestEntityUniqueId> unique_ids;

    // Unique entity data
    TestEntityUniqueEntityData unique_entities;

    // Queued updates
    EntityData queued_entity_data;
    TArray<FRegistryEntityHandle> queued_entity_update_handles;
    EntityDeathInfo queued_death_infos;

    // Queued damage events
    DirectDamageEvents queued_direct_damage_events;

    // Dead entities
    TArray<FRegistryEntityHandle> dead_entities_this_frame;
    TArray<int32> free_indices;
};

inline auto FTestEntityRegistry::is_valid_handle(FRegistryEntityHandle const index) const -> bool {
    return generations.IsValidIndex(index.index) && (generations[index.index] == index.generation);
}

inline auto FTestEntityRegistry::is_valid_alive(FRegistryEntityHandle const handle) const -> bool {
    return is_valid_handle(handle) && (entity_data.alive[handle.index] > 0);
}
inline auto FTestEntityRegistry::is_valid_dead(FRegistryEntityHandle const handle) const -> bool {
    return is_valid_handle(handle) && (entity_data.alive[handle.index] == 0);
}
