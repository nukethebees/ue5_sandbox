#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassSubsystemBase.h"

#include "MassBulletSubsystem.generated.h"

UCLASS()
class SANDBOX_API UMassBulletSubsystem : public UMassSubsystemBase {
    GENERATED_BODY()
  public:
    void add_bullet(FTransform const& transform, float velocity);
    void return_bullet(FMassEntityHandle handle);
  private:
    TArray<FMassEntityHandle> free_list;
};
