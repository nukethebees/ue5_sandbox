#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Sandbox/players/playable/CoinCollectorActorComponent.h"

#include "PlatformerGameState.generated.h"

UCLASS()
class SANDBOX_API APlatformerGameState : public AGameStateBase {
    GENERATED_BODY()
  public:
    void notify_coin_change(int32 new_coin_count);

    FCoinCountChangedSignature on_coin_count_changed;
};
