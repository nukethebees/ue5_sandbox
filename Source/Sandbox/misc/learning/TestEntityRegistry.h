#pragma once

#include "TestEntityRegistryData.h"

#include "Sandbox/misc/learning/TestEntityOwnerId.h"
#include "Sandbox/misc/learning/TestTeam.h"

#include "SandboxCore/generation_index.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <array>

#include "TestEntityRegistry.generated.h"

class UActorComponent;

struct TraceHits {
    TArray<AActor*> actors;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;

    void reset();
    void validate_array_sizes() const;
    auto num() const -> int32;
    void
        remove_at_swap(int32 const index, int32 const count, EAllowShrinking const allow_shrinking);
};
struct DamageEvents {
    TArray<int32> damage_amounts;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;

    auto num() const -> int32;
    void reset();
    void validate_array_sizes() const;
    void
        remove_at_swap(int32 const index, int32 const count, EAllowShrinking const allow_shrinking);
};

UCLASS()
class ATestEntityRegistry : public AActor {
  public:
    GENERATED_BODY()

    struct ConstView {
        auto get_num() const { return indices.Num(); }

        TConstArrayView<FGenerationIndex> indices;
        FTestEntityRegistryEntityData::ConstView data;
    };
    struct View {
        auto get_num() const { return indices.Num(); }

        TConstArrayView<FGenerationIndex> indices;
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
    auto reserve_entities(int32 const count) -> TArray<FGenerationIndex>;
    auto add_entities(FTestEntityRegistryEntityData::ConstView const view)
        -> TArray<FGenerationIndex>;

    // Damage updates
    void queue_damage_events(TConstArrayView<int32> const damages,
                             TConstArrayView<AActor*> const actors,
                             TConstArrayView<UActorComponent*> const components,
                             TConstArrayView<int32> const items);
    auto get_damage_queue_view(TestEntityOwnerId const id) const -> DamageEvents const&;
    void filter_damage_candidates();

    // General entity updates
    void update_entities(ConstView const view);

    // Frame events
    void commit_updates();
    void end_tick();

    // Index queries
    auto is_valid_index(FGenerationIndex const index) const -> bool;
    auto is_stale(FGenerationIndex const index) const -> bool;

    // Entity queries
    auto get_num_elements() const noexcept -> int32;
    auto get_location(FGenerationIndex const index) const -> FVector3f;
    auto get_velocity(FGenerationIndex const index) const -> FVector3f;
    auto get_health(FGenerationIndex const index) const -> int32;
    auto get_team(FGenerationIndex const index) const -> ETestTeam;
    auto get_alive(FGenerationIndex const index) const -> bool;
    auto get_dead_entities_this_frame() const -> TConstArrayView<FGenerationIndex>;

    // Area queries
    auto collect_entities_in_range(FVector3f const& origin,
                                   float const radius,
                                   TArrayView<FGenerationIndex> const out_entities) const -> int32;
    auto collect_non_team_entities_in_range(FVector3f const& origin,
                                            ETestTeam const team,
                                            float const radius,
                                            TArrayView<FGenerationIndex> const out_entities) const
        -> int32;

    // Checks
    void validate_array_sizes() const;
  private:
    void commit_entity_updates();
    void commit_death_updates();
    void refresh_free_indices();

    FTestEntityRegistryEntityData entity_data;
    TArray<int32> generations;
    TArray<AActor const*> entity_owners;

    // Queued updates
    FTestEntityRegistryEntityData queued_entity_data;
    TArray<FGenerationIndex> queued_entity_generations;

    // Queued damage events
    TArray<DamageEvents> queued_damage_events;
    TArray<int32> damage_events_to_filter_buffer;

    // Dead entities
    TArray<FGenerationIndex> dead_entities_this_frame;
    TArray<int32> free_indices;
};
