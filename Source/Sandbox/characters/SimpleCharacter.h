#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Sandbox/interfaces/DeathHandler.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "SimpleCharacter.generated.h"

UENUM(BlueprintType)
enum class ESimpleCharacterAttackMode : uint8 {
    None UMETA(DisplayName = "None"),
    Melee UMETA(DisplayName = "Melee"),
    Ranged UMETA(DisplayName = "Ranged")
};

class UHealthComponent;

UCLASS()
class SANDBOX_API ASimpleCharacter
    : public ACharacter
    , public ml::LogMsgMixin<"ASimpleCharacter", LogSandboxCharacter>
    , public IDeathHandler {
    GENERATED_BODY()
  public:
    ASimpleCharacter();

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESimpleCharacterAttackMode atatck_mode{ESimpleCharacterAttackMode::None};
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
