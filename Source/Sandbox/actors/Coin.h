#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Coin.generated.h"

UCLASS()
class SANDBOX_API ACoin : public AActor {
    GENERATED_BODY()
  public:
    ACoin();
  protected:
    virtual void BeginPlay() override;
    virtual void NotifyActorBeginOverlap(AActor* other_actor) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coin")
    float rotation_speed{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coin")
    int32 coin_value{1};
};
