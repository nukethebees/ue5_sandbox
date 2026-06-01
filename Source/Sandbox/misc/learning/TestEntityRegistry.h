#pragma once

#include "TestTeam.h"

#include "SandboxCore/generation_index.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <array>

#include "TestEntityRegistry.generated.h"

USTRUCT()
struct FTestEntityRegistryEntityData {
    GENERATED_BODY()

    template <template <typename> typename TView>
    struct TTestEntityDataView {
        using ThisClass = TTestEntityDataView<TView>;

        TView<FVector> locations;
        TView<FVector> velocities;
        TView<int32> healths;
        TView<ETestTeam> teams;
        TView<uint8> alive;

        auto get_num() const -> int32 { return locations.Num(); }
        auto get_slice(int32 const offset, int32 const count) const {
            return ThisClass{
                locations.Slice(offset, count),
                velocities.Slice(offset, count),
                healths.Slice(offset, count),
                teams.Slice(offset, count),
                alive.Slice(offset, count),
            };
        }
    };

    using View = TTestEntityDataView<TArrayView>;
    using ConstView = TTestEntityDataView<TConstArrayView>;

    FTestEntityRegistryEntityData() = default;

    auto get_num() const -> int32;
    auto get_view() -> View;
    auto get_const_view() const -> ConstView;

    void add_uninitialised(int32 const count);
    void add_disabled(int32 const count);
    void add(ConstView const view);

    UPROPERTY()
    TArray<FVector> locations;
    UPROPERTY()
    TArray<FVector> velocities;
    UPROPERTY()
    TArray<int32> healths;
    UPROPERTY()
    TArray<ETestTeam> teams;
    UPROPERTY()
    TArray<uint8> alive;
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

    // Entity updates
    auto reserve_entities(int32 const count) -> TArray<FGenerationIndex>;
    void update_entities(ConstView const view);
    auto add_entities(FTestEntityRegistryEntityData::ConstView const view)
        -> TArray<FGenerationIndex>;

    // Entity queries
    auto is_valid_index(FGenerationIndex const index) const -> bool;
    auto get_num_elements() const noexcept -> int32;
    auto get_location(FGenerationIndex const index) const -> FVector;
    auto get_velocity(FGenerationIndex const index) const -> FVector;
    auto get_health(FGenerationIndex const index) const -> int32;
    auto get_team(FGenerationIndex const index) const -> ETestTeam;
    auto get_alive(FGenerationIndex const index) const -> bool;

    auto collect_entities_in_range(FVector const& origin,
                                   float const radius,
                                   TArrayView<FGenerationIndex> const out_entities) const -> int32;
  private:
    UPROPERTY()
    FTestEntityRegistryEntityData entity_data;
    UPROPERTY()
    TArray<int32> generations;

    UPROPERTY();
    TArray<int32> free_indices;
    std::array<TArray<int32>, TEAM_COUNT> indices_by_team;
};
