#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Sandbox/data/TeamID.h"
#include "Sandbox/interfaces/DeathHandler.h"
#include "Sandbox/interfaces/SandboxMobInterface.h"
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
    , public IDeathHandler
    , public IGenericTeamAgentInterface
    , public ISandboxMobInterface {
    GENERATED_BODY()
  public:
    ASimpleCharacter();

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESimpleCharacterAttackMode attack_mode{ESimpleCharacterAttackMode::None};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor mesh_base_colour{FLinearColor::Green};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor mesh_emissive_colour{FLinearColor::Black};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor light_colour{FLinearColor::White};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETeamID team_id{ETeamID::Enemy};

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // ISandboxMobInterface
    virtual UBehaviorTree* get_behaviour_tree_asset() const override;
    virtual float get_acceptable_radius() const override;
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UBehaviorTree* behaviour_tree_asset{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float acceptable_radius{100.0f};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* body_mesh{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UPointLightComponent* light{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UHealthComponent* health{nullptr};
  private:
    // IDeathHandler
    virtual void handle_death() override;

    void apply_material_colours();
    void apply_light_colours();

    UMaterialInstanceDynamic* dynamic_material{nullptr};
};
