// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "KillAllEnemiesGameMode.generated.h"

UCLASS()
class SANDBOX_API AKillAllEnemiesGameMode
    : public AGameModeBase
    , public ml::LogMsgMixin<"AKillAllEnemiesGameMode", LogSandboxCore> {
    GENERATED_BODY()
  public:
    AKillAllEnemiesGameMode();
  protected:
    virtual void
        InitGame(FString const& MapName, FString const& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;

    void handle_enemy_death();

    UPROPERTY(VisibleAnywhere, Category = "Game")
    int32 enemies_left{0};
};
