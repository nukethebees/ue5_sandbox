#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "SimpleCharacter.generated.h"

UCLASS()
class SANDBOX_API ASimpleCharacter
    : public ACharacter
    , public ml::LogMsgMixin<"ASimpleCharacter"> {
    GENERATED_BODY()
  public:
    ASimpleCharacter();
};
