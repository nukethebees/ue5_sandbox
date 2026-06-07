// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Sandbox/game_flow/SandboxGameMode.h"

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "SpaceShipGameMode.generated.h"

class APawn;

UCLASS()
class SANDBOX_API ASpaceShipGameMode : public ASandboxGameMode {
    GENERATED_BODY()
  public:
    ASpaceShipGameMode();
  protected:
    void InitGame(FString const& MapName, FString const& Options, FString& ErrorMessage) override;
    void BeginPlay() override;
};
