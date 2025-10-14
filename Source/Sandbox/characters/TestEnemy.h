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

#include "Sandbox/data/ai/CombatProfile.h"
#include "Sandbox/enums/MobAttackMode.h"
#include "Sandbox/enums/TeamID.h"
#include "Sandbox/interfaces/CombatActor.h"
#include "Sandbox/interfaces/DeathHandler.h"
#include "Sandbox/interfaces/SandboxMobInterface.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "TestEnemy.generated.h"

class UHealthComponent;

UCLASS()
class SANDBOX_API ATestEnemy
    : public ACharacter
    , public ml::LogMsgMixin<"ATestEnemy", LogSandboxCharacter>
    , public ICombatActor
    , public IDeathHandler
    , public IGenericTeamAgentInterface
    , public ISandboxMobInterface {
    GENERATED_BODY()
  public:
    ATestEnemy();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_base_colour{FLinearColor::Green};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor mesh_emissive_colour{FLinearColor::Black};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor light_colour{FLinearColor::White};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    ETeamID team_id{ETeamID::Enemy};

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // ICombatActor
    virtual bool attack_actor(AActor* target) override;

    // ISandboxMobInterface
    virtual UBehaviorTree* get_behaviour_tree_asset() const override;
    virtual float get_acceptable_radius() const override;
    virtual float get_attack_radius() const override;
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    UBehaviorTree* behaviour_tree_asset{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    FCombatProfile combat_profile{EMobAttackMode::None, 150.0f, 1000.0f};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
    UStaticMeshComponent* body_mesh{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
    UPointLightComponent* light{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UHealthComponent* health{nullptr};
  private:
    // IDeathHandler
    virtual void handle_death() override;

    void apply_material_colours();
    void apply_light_colours();

    UMaterialInstanceDynamic* dynamic_material{nullptr};
    float last_attack_time{0.0f};
};
