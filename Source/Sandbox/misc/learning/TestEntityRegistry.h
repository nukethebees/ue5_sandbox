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

    struct QueuedDamageResolveView {
        TArrayView<FGenerationIndex> targets;

        TConstArrayView<int32> damage_amounts;
        TConstArrayView<AActor*> damaged_actors;
        TConstArrayView<UActorComponent*> damaged_actor_components;
        TConstArrayView<int32> damaged_hit_items; // Generally an ISMC index

        auto num() const -> int32;
        void check_lengths() const;
    };

    static constexpr uint8 TEAM_COUNT{static_cast<uint8>(ETestTeam::COUNT)};

    ATestEntityRegistry();

    void reset();

    // Registration
    auto register_owner(AActor const& actor) -> TestEntityOwnerId;

    // Entity creation
    auto reserve_entities(int32 const count) -> TArray<FGenerationIndex>;
    auto add_entities(FTestEntityRegistryEntityData::ConstView const view)
        -> TArray<FGenerationIndex>;

    // Entity updates
    auto get_damage_queue_view() -> QueuedDamageResolveView;
    void update_entities(ConstView const view);
    void apply_damage(TConstArrayView<int32> const damages,
                      TConstArrayView<AActor*> const actors,
                      TConstArrayView<UActorComponent*> const components,
                      TConstArrayView<int32> const items);

    // Frame events
    void commit_updates();
    void end_frame();

    // Entity queries
    auto is_valid_index(FGenerationIndex const index) const -> bool;
    auto get_num_elements() const noexcept -> int32;
    auto get_location(FGenerationIndex const index) const -> FVector;
    auto get_velocity(FGenerationIndex const index) const -> FVector;
    auto get_health(FGenerationIndex const index) const -> int32;
    auto get_team(FGenerationIndex const index) const -> ETestTeam;
    auto get_alive(FGenerationIndex const index) const -> bool;
    auto get_dead_entities_this_frame() const -> TConstArrayView<FGenerationIndex>;
    auto collect_entities_in_range(FVector const& origin,
                                   float const radius,
                                   TArrayView<FGenerationIndex> const out_entities) const -> int32;
    auto collect_non_team_entities_in_range(FVector const& origin,
                                            ETestTeam const team,
                                            float const radius,
                                            TArrayView<FGenerationIndex> const out_entities) const
        -> int32;
  private:
    void commit_entity_updates();
    void commit_damage_updates();
    void commit_death_updates();
    void refresh_free_indices();

    UPROPERTY()
    FTestEntityRegistryEntityData entity_data;
    UPROPERTY()
    TArray<int32> generations;
    UPROPERTY()
    TArray<AActor const*> entity_owners;

    // Queued updates
    UPROPERTY()
    FTestEntityRegistryEntityData queued_entity_data;
    UPROPERTY()
    TArray<FGenerationIndex> queued_entity_generations;

    // Queued damage events
    UPROPERTY()
    TArray<int32> queued_damage_amounts;
    UPROPERTY()
    TArray<AActor*> queued_damaged_actors;
    UPROPERTY()
    TArray<UActorComponent*> queued_damaged_actor_components;
    UPROPERTY()
    TArray<int32> queued_damaged_hit_items;
    UPROPERTY()
    TArray<FGenerationIndex> queued_damage_targets;

    // Dead entities
    UPROPERTY()
    TArray<FGenerationIndex> dead_entities_this_frame;
    UPROPERTY();
    TArray<int32> free_indices;
};
