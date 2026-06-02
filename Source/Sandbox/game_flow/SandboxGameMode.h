#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "SandboxGameMode.generated.h"

UCLASS()
class SANDBOX_API ASandboxGameMode : public AGameModeBase {
    GENERATED_BODY()
  public:
    ASandboxGameMode();
  protected:
    virtual void
        InitGame(FString const& MapName, FString const& Options, FString& ErrorMessage) override;
    void BeginPlay() override;
};
