#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/data/SpeedBoost.h"
#include "Sandbox/interfaces/CollisionEffectComponent.h"

#include "SpeedBoostItemComponent.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class SANDBOX_API USpeedBoostItemComponent
    : public UActorComponent
    , public ICollisionEffectComponent {
    GENERATED_BODY()
  public:
    USpeedBoostItemComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FSpeedBoost speed_boost{};

    // ICollisionEffectComponent implementation
    virtual void execute_effect(AActor* other_actor) override;
  protected:
    virtual void BeginPlay() override;
};
