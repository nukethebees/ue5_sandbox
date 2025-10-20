#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Sandbox/players/playable/actor_components/CoinCollectorActorComponent.h"

#include "PlatformerGameState.generated.h"

UCLASS()
class SANDBOX_API APlatformerGameState : public AGameStateBase {
    GENERATED_BODY()
  public:
    UPROPERTY(BlueprintAssignable, Category = "Coin")
    FCoinCountChangedSignature on_coin_count_changed;

    void notify_coin_change(int32 new_coin_count);
};
