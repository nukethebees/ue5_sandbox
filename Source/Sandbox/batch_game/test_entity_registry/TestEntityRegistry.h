#pragma once

#include "TestEntityRegistryData.h"

#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryHandleState.h>
#include <Sandbox/batch_game/test_entity_registry/TestDeathReason.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueEntityData.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxCore/array_utils.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestEntityRegistry.generated.h"

class UActorComponent;

struct DamageEvents;
struct UnresolvedDamageEvents;

struct NewEntities {
    TArray<FRegistryEntityHandle> registry_handles;
    TestEntityUniqueId first_id;

    auto num() const -> int32;
    void reset();
    void add_defaulted(int32 const count);
    void add_uninitialised(int32 const count);
};

UCLASS()
class SANDBOX_API ATestEntityRegistry : public AActor {
  public:
    GENERATED_BODY()

    struct ConstView {
        auto get_num() const { return indices.Num(); }

        TConstArrayView<FRegistryEntityHandle> indices;
        FTestEntityRegistryEntityData::ConstView data;
    };
    struct View {
        auto get_num() const { return indices.Num(); }

        TConstArrayView<FRegistryEntityHandle> indices;
        FTestEntityRegistryEntityData::View data;
    };

    static constexpr uint8 TEAM_COUNT{static_cast<uint8>(ETestTeam::COUNT)};

    ATestEntityRegistry();

    void reset();

    // Getters
    auto get_unique_entities() const noexcept -> TestEntityUniqueEntityData const& {
        return unique_entities;
    }

    // Registration
    auto register_owner(AActor const& actor) -> TestEntityOwnerId;
    auto is_owner(AActor const* const actor) const -> bool;
    auto is_valid_owner(TestEntityOwnerId const id) const -> bool;
    auto get_owner(AActor const* const actor) -> TestEntityOwnerId;

    // Entity creation
    auto add_entities(FTestEntityRegistryEntityData::ConstView const view) -> NewEntities;

    // Damage updates
    void queue_damage_events(UnresolvedDamageEvents const& damage_events);
    auto get_damage_queue_view(TestEntityOwnerId const id) const -> DamageEvents const&;

    // General entity updates
    void queue_entity_updates(ConstView const view, EntityDeathInfo const& death_info);

    // Frame events
    void commit_updates();
    void end_tick();

    // Handle queries
    auto analyse_handle(FRegistryEntityHandle const handle) const -> ERegistryHandleState;
    auto is_valid_handle(FRegistryEntityHandle const index) const -> bool;
    auto is_stale(FRegistryEntityHandle const index) const -> bool;

    // Handle updates
    void refresh_handles(TArrayView<FRegistryEntityHandle> handles) const;

    // Entity queries
    auto get_num_elements() const noexcept -> int32;
    auto get_num_alive_active_entities() const noexcept -> int32;

    auto get_entity_data() const noexcept -> FTestEntityRegistryEntityData const& {
        return entity_data;
    }
    auto get_active_unique_ids() const noexcept -> TConstArrayView<TestEntityUniqueId> {
        return unique_ids;
    }

    auto get_generations() const noexcept -> TConstArrayView<int> { return generations; }
    auto get_location(FRegistryEntityHandle const index) const -> FVector3f;
    auto get_velocity(FRegistryEntityHandle const index) const -> FVector3f;
    auto get_health(FRegistryEntityHandle const index) const -> int32;
    auto get_team(FRegistryEntityHandle const index) const -> ETestTeam;
    auto get_alive(FRegistryEntityHandle const index) const -> bool;
    auto get_dead_entities_this_frame() const -> TConstArrayView<FRegistryEntityHandle>;

    auto is_valid_alive(FRegistryEntityHandle const handle) const -> bool;

    // Total queries
    auto count_kills() const noexcept -> int32;
    auto count_alive() const noexcept -> int32;
    auto count_alive_not_on_team(ETestTeam const team) const noexcept -> int32;

    // Unique id queries
    auto is_valid_unique_id(TestEntityUniqueId const id) const -> bool;
    auto get_num_unique_ids_issued() const -> int32 { return ml::num(unique_entities); }
    auto find_unique_id(FRegistryEntityHandle const handle) const -> TestEntityUniqueId;

    auto get_kills(TestEntityUniqueId const id) const -> TestEntityUniqueEntityData::kills_type;

    // Area queries
    auto collect_entities_in_range(FVector3f const& origin,
                                   float const radius,
                                   TArrayView<FRegistryEntityHandle> const out_entities) const
        -> int32;
    auto collect_non_team_entities_in_range(
        FVector3f const& origin,
        ETestTeam const team,
        float const radius,
        TArrayView<FRegistryEntityHandle> const out_entities) const -> int32;
    auto get_any_non_team_entity(ETestTeam const team) const -> FRegistryEntityHandle;
    auto get_any_non_team_entity(ETestTeam const team, ETestEntityType const entity_type) const
        -> FRegistryEntityHandle;
    void are_entities_within_dist_sq(float const dist_sq,
                                     FVectors3f const& locations,
                                     TArrayView<bool> results);

    // Checks
    void validate_array_sizes() const;
  private:
    void commit_entity_updates();
    void commit_death_updates();
    void refresh_free_indices();

    void validate_unique_ids() const;
    void validate_unique_entity_data() const;

    FTestEntityRegistryEntityData entity_data;
    TArray<int32> generations;
    TArray<TestEntityUniqueId> unique_ids;
    TArray<AActor const*> entity_owners;

    TestEntityUniqueEntityData unique_entities;

    // Queued updates
    FTestEntityRegistryEntityData queued_entity_data;
    TArray<FRegistryEntityHandle> queued_entity_update_handles;
    EntityDeathInfo queued_death_infos;

    // Queued damage events
    TArray<DamageEvents> queued_damage_events;
    TArray<int32> damage_events_to_filter_buffer;

    // Dead entities
    TArray<FRegistryEntityHandle> dead_entities_this_frame;
    TArray<int32> free_indices;
};

inline auto ATestEntityRegistry::is_valid_handle(FRegistryEntityHandle const index) const -> bool {
    return generations.IsValidIndex(index.index) && (generations[index.index] == index.generation);
}

inline auto ATestEntityRegistry::is_valid_alive(FRegistryEntityHandle const handle) const -> bool {
    return is_valid_handle(handle) && (entity_data.alive[handle.index] > 0);
}
