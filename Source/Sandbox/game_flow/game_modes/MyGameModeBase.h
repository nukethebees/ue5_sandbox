// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/core/destruction/actor_components/RemoveGhostsOnStartComponent.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/players/playable/characters/MyCharacter.h"
#include "Sandbox/ui/hud/huds/MyHud.h"

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
