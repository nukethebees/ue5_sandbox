#pragma once

#include "GameFramework/GameModeBase.h"

#include "MainMenuGameMode.generated.h"

namespace ml::ioj {
UCLASS()
class SPACEGAME_API AMainMenuGameMode : public AGameModeBase {
    GENERATED_BODY()
  public:
    AMainMenuGameMode();

    void StartPlay() override;
    void PostLogin(APlayerController* new_player) override;
    void EndPlay(EEndPlayReason::Type reason) override;
};
}
