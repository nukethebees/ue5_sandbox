// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "SimpleCharacter.generated.h"

UCLASS()
class SANDBOX_API ASimpleCharacter
    : public ACharacter
    , public ml::LogMsgMixin<"SimpleCharacter"> {
    GENERATED_BODY()
  public:
    ASimpleCharacter();
};
