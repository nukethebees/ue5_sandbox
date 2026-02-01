// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "SpaceShipGameMode.generated.h"

UCLASS()
class SANDBOX_API ASpaceShipGameMode : public AGameModeBase {
    GENERATED_BODY()
  public:
    ASpaceShipGameMode();
  protected:
    void InitGame(FString const& MapName, FString const& Options, FString& ErrorMessage) override;
    void BeginPlay() override;
};
