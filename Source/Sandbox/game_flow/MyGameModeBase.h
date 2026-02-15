// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Sandbox/core/destruction/RemoveGhostsOnStartComponent.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/TeamID.h"
#include "Sandbox/players/playable/MyCharacter.h"

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

#include "MyGameModeBase.generated.h"

UCLASS()
class SANDBOX_API AMyGameModeBase
    : public AGameModeBase
    , public ml::LogMsgMixin<"AMyGameModeBase", LogSandboxCore> {
    GENERATED_BODY()
  public:
    AMyGameModeBase();
  protected:
    virtual void
        InitGame(FString const& MapName, FString const& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Mode")
    URemoveGhostsOnStartComponent* ghost_cleanup_component{nullptr};
};
