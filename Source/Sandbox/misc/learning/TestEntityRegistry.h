#pragma once

#include "SandboxCore/generation_index.h"
#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <array>

#include "TestEntityRegistry.generated.h"

USTRUCT()
struct FTestEntityRegistryEntityData {
    GENERATED_BODY()

    struct ConstView {
        TConstArrayView<FVector> locations;
        TConstArrayView<FVector> velocities;
        TConstArrayView<int32> healths;
        TConstArrayView<ETestTeam> teams;
        TConstArrayView<uint8> alive;
    };
    struct View {
        TArrayView<FVector> locations;
        TArrayView<FVector> velocities;
        TArrayView<int32> healths;
        TArrayView<ETestTeam> teams;
        TArrayView<uint8> alive;
    };

    FTestEntityRegistryEntityData() = default;

    auto get_num() const -> int32;
    auto get_view() -> View;
    auto get_const_view() const -> ConstView;

    void add_uninitialised(int32 const count);
    void add_disabled(int32 const count);

    UPROPERTY(VisibleAnywhere)
    TArray<FVector> locations;
    UPROPERTY(VisibleAnywhere)
    TArray<FVector> velocities;
    UPROPERTY(VisibleAnywhere)
    TArray<int32> healths;
    UPROPERTY(VisibleAnywhere)
    TArray<ETestTeam> teams;
    UPROPERTY(VisibleAnywhere)
    TArray<uint8> alive;
};

UCLASS()
class ATestEntityRegistry : public AActor {
    GENERATED_BODY()
  public:
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

    // Entity updates
    auto reserve_entities(int32 const count) -> TArray<FGenerationIndex>;
    void update_entities(ConstView const view);

    // Entity queries
    auto is_valid_index(FGenerationIndex const index) const -> bool;
    auto get_num_elements() const noexcept -> int32;
    auto collect_entities_in_range(FVector const& origin,
                                   float radius,
                                   TArrayView<FGenerationIndex> out_entities) const -> int32;
  private:
    UPROPERTY(VisibleAnywhere, Category = "Registry")
    FTestEntityRegistryEntityData entity_data;
    UPROPERTY(VisibleAnywhere, Category = "Registry")
    TArray<int32> generations;

    UPROPERTY();
    TArray<int32> free_indices;
    std::array<TArray<int32>, TEAM_COUNT> indices_by_team;
};
