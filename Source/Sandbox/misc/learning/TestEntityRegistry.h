#pragma once

#include "TestEntityRegistryData.h"
#include "TestTeam.h"

#include "SandboxCore/generation_index.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <array>

#include "TestEntityRegistry.generated.h"

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

    // Entity creation
    auto reserve_entities(int32 const count) -> TArray<FGenerationIndex>;
    auto add_entities(FTestEntityRegistryEntityData::ConstView const view)
        -> TArray<FGenerationIndex>;

    // Entity updates
    void update_entities(ConstView const view);
    void apply_damage(TConstArrayView<FGenerationIndex> const indexes,
                      TConstArrayView<int32> const damages);

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

    // Queued updates
    UPROPERTY()
    FTestEntityRegistryEntityData queued_entity_data;
    UPROPERTY()
    TArray<FGenerationIndex> queued_entity_generations;

    // Queued damage events
    UPROPERTY()
    TArray<int32> queued_damage_amounts;
    UPROPERTY()
    TArray<FGenerationIndex> queued_damage_targets;

    // Dead entities
    UPROPERTY()
    TArray<FGenerationIndex> dead_entities_this_frame;

    UPROPERTY();
    TArray<int32> free_indices;
};
