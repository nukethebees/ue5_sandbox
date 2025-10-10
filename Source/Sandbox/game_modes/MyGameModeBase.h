// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/actor_components/RemoveGhostsOnStartComponent.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/huds/MyHud.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MyGameModeBase.generated.h"

UCLASS()
class SANDBOX_API AMyGameModeBase
    : public AGameModeBase
    , public ml::LogMsgMixin<"AMyGameModeBase"> {
    GENERATED_BODY()
  public:
    AMyGameModeBase();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Mode")
    URemoveGhostsOnStartComponent* ghost_cleanup_component{nullptr};
};
