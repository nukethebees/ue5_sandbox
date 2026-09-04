#pragma once

#include "GameFramework/GameModeBase.h"

#include "MainMenuGameMode.generated.h"

namespace ml::ioj {
UCLASS()
class SPACEGAME_API AMainMenuGameMode : public AGameModeBase {
    GENERATED_BODY()
  public:
    AMainMenuGameMode();

    void PostLogin(APlayerController* new_player) override;
};
}
