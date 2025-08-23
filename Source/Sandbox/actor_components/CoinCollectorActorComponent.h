#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CoinCollectorActorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UCoinCollectorActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UCoinCollectorActorComponent();

    auto coin_count() const { return coin_count_; }
    auto add_coins(int32 number = 1) { coin_count_ += number; }
  protected:
    virtual void BeginPlay() override;
  private:
    int32 coin_count_{0};
};
