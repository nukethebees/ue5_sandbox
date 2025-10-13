#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MassBulletSubsystemData.generated.h"

class UBulletDataAsset;

UCLASS()
class SANDBOX_API AMassBulletSubsystemData : public AActor {
    GENERATED_BODY()
  public:
    AMassBulletSubsystemData();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    TArray<TObjectPtr<UBulletDataAsset>> bullet_types;
};
