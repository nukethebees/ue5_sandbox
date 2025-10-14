#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Sandbox/interfaces/DeathHandler.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "SimpleCharacter.generated.h"

class UHealthComponent;

UCLASS()
class SANDBOX_API ASimpleCharacter
    : public ACharacter
    , public ml::LogMsgMixin<"ASimpleCharacter", LogSandboxCharacter>
    , public IDeathHandler {
    GENERATED_BODY()
  public:
    ASimpleCharacter();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UHealthComponent* health{nullptr};
  private:
    // IDeathHandler
    virtual void handle_death() override;
};
