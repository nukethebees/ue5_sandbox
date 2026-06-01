#pragma once

#include "SandboxCore/generation_index.h"
#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <array>

#include "TestEntityRegistry.generated.h"

UCLASS()
class ATestEntityRegistry : public AActor {
    GENERATED_BODY()
  public:
    static constexpr uint8 TEAM_COUNT{static_cast<uint8>(ETestTeam::COUNT)};

    ATestEntityRegistry();

    auto collect_entities_in_range(FVector const& origin,
                                   float radius,
                                   TArrayView<FGenerationIndex> out_entities) -> int32;
    auto get_num_elements() const noexcept -> int32;

    auto reserve_entities(int32 const count) -> TArray<FGenerationIndex>;
  private:
    UPROPERTY()
    TArray<FVector> locations;
    UPROPERTY()
    TArray<FVector> velocities;
    UPROPERTY()
    TArray<int32> healths;
    UPROPERTY()
    TArray<int32> generations;
    UPROPERTY()
    TArray<ETestTeam> teams;
    UPROPERTY()
    TArray<uint8> alive;

    TArray<int32> free_indices;
    std::array<TArray<int32>, TEAM_COUNT> indices_by_team;
};
