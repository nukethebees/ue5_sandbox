#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "NpcPatrolComponent.generated.h"

class APatrolPath;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UNpcPatrolComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UNpcPatrolComponent();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "AI")
    APatrolPath* patrol_path{nullptr};
};
