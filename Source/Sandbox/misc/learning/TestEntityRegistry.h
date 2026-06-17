#pragma once

#include "TestEntityRegistryData.h"

#include <Sandbox/misc/learning/RegistryEntityHandle.h>
#include <Sandbox/misc/learning/TestDeathReason.h>
#include <Sandbox/misc/learning/TestEntityOwnerId.h>
#include <Sandbox/misc/learning/TestEntityUniqueId.h>
#include <Sandbox/misc/learning/TestTeam.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <array>

#include "TestEntityRegistry.generated.h"

class UActorComponent;

enum class ERegistryHandleState : uint8 {
    Active,
    Stale,
    Invalid,
};

struct TraceHits {
    TArray<AActor*> actors;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;

    void reset();
    auto num() const -> int32;
    void
        remove_at_swap(int32 const index, int32 const count, EAllowShrinking const allow_shrinking);

    void validate_array_sizes() const;
};

struct UnresolvedDamageEvents {
    TArray<AActor*> damaged_actors;
    TArray<int32> damage_amounts;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;
    TArray<FRegistryEntityHandle> instigators;

    auto num() const -> int32;
    void reset();
    void validate_array_sizes() const;
};

struct DamageEvents {
    TArray<int32> damage_amounts;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;
    TArray<FRegistryEntityHandle> instigators;

    auto num() const -> int32;
    void reset();
    void
        remove_at_swap(int32 const index, int32 const count, EAllowShrinking const allow_shrinking);

    void validate_array_sizes() const;
};

struct TestEntityUniqueEntityData {
    using kills_type = uint32;

    TArray<FRegistryEntityHandle> registry_handles;
    TArray<kills_type> kills;
    TArray<uint8> alive;
    TArray<TestEntityUniqueId> killed_by;
    TArray<ETestDeathReason> death_reason;

    auto num() const -> int32;
    void reset();
    void add_defaulted(int32 const count);

    void validate_array_sizes() const;
};

struct NewEntities {
    TArray<FRegistryEntityHandle> registry_handles;
    TestEntityUniqueId first_id;

    auto num() const -> int32;
    void reset();
    void add_defaulted(int32 const count);
    void add_uninitialised(int32 const count);
};

struct EntityDeathInfo {
    TArray<ETestDeathReason> reasons;
    TArray<FRegistryEntityHandle> victims;
    TArray<FRegistryEntityHandle> killers;

    void add(ETestDeathReason const reason,
             FRegistryEntityHandle const victim,
             FRegistryEntityHandle const killer);
    void add(ETestDeathReason const reason, FRegistryEntityHandle const victim) {
        add(reason, victim, FRegistryEntityHandle{});
    }

    auto num() const -> int32;
    void reset();

    void validate_array_sizes() const;
};

UCLASS()
class ATestEntityRegistry : public AActor {
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

    // Registration
    auto register_owner(AActor const& actor) -> TestEntityOwnerId;
    auto is_owner(AActor const* const actor) const -> bool;
    auto is_valid_owner(TestEntityOwnerId const id) const -> bool;
    auto get_owner(AActor const* const actor) -> TestEntityOwnerId;

    // Entity creation
    auto reserve_entities(int32 const count) -> NewEntities;
    auto add_entities(FTestEntityRegistryEntityData::ConstView const view) -> NewEntities;

    // Damage updates
    void queue_damage_events(UnresolvedDamageEvents const& damage_events);
    auto get_damage_queue_view(TestEntityOwnerId const id) const -> DamageEvents const&;

    // General entity updates
    void update_entities(ConstView const view);
    void set_death_infos(EntityDeathInfo const& death_info);

    // Frame events
    void commit_updates();
    void end_tick();

    // Handle queries
    auto analyse_handle(FRegistryEntityHandle const handle) const -> ERegistryHandleState;
    auto is_valid_index(FRegistryEntityHandle const index) const -> bool;
    auto is_stale(FRegistryEntityHandle const index) const -> bool;

    // Entity queries
    auto get_num_elements() const noexcept -> int32;
    auto get_location(FRegistryEntityHandle const index) const -> FVector3f;
    auto get_velocity(FRegistryEntityHandle const index) const -> FVector3f;
    auto get_health(FRegistryEntityHandle const index) const -> int32;
    auto get_team(FRegistryEntityHandle const index) const -> ETestTeam;
    auto get_alive(FRegistryEntityHandle const index) const -> bool;
    auto get_dead_entities_this_frame() const -> TConstArrayView<FRegistryEntityHandle>;

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

    // Queued damage events
    TArray<DamageEvents> queued_damage_events;
    TArray<int32> damage_events_to_filter_buffer;

    // Dead entities
    TArray<FRegistryEntityHandle> dead_entities_this_frame;
    TArray<int32> free_indices;
};
