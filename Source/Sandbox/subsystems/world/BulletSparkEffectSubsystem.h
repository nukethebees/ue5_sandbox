#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "BulletSparkEffectSubsystem.generated.h"

UCLASS()
class SANDBOX_API UBulletSparkEffectSubsystem : public UWorldSubsystem {
    GENERATED_BODY()
  public:
    void add_impact(FVector const& location, FRotator const& rotation);
  private:
    TArray<FVector> impact_locations;
    TArray<FRotator> impact_rotations;
};
