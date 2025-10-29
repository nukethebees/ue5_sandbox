#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CoinCollectorActorComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FCoinCountChangedSignature, int32);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UCoinCollectorActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UCoinCollectorActorComponent();

    auto coin_count() const { return coin_count_; }
    void add_coins(int32 number = 1);

    FCoinCountChangedSignature on_coin_count_changed;
  protected:
    virtual void BeginPlay() override;
  private:
    int32 coin_count_{0};
};
