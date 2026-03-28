// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Sandbox/game_flow/SpaceShipGameMode.h"

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "KatinaGameMode.generated.h"

class AMothershipBoss;

UCLASS()
class AKatinaGameMode : public ASpaceShipGameMode {
    GENERATED_BODY()
  public:
    AKatinaGameMode();
  protected:
    void InitGame(FString const& MapName, FString const& Options, FString& ErrorMessage) override;
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    UFUNCTION()
    void on_boss_killed(AActor* actor);

    AMothershipBoss* boss{nullptr};
    FDelegateHandle on_boss_killed_delegate;
};
