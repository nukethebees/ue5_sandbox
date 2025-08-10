// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "MyCharacter.h"
#include "Sandbox/mixins/print_msg_mixin.hpp"
#include "Sandbox/MyHud.h"

#include "MyGameModeBase.generated.h"

UCLASS()
class SANDBOX_API AMyGameModeBase
    : public AGameModeBase
    , public print_msg_mixin {
    GENERATED_BODY()
  protected:
    virtual void BeginPlay() override;
};
