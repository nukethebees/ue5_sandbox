#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#include "Sandbox/health/interfaces/DeathHandler.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/players/npcs/enums/AIState.h"
#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"
#include "Sandbox/utilities/enums.h"

#include "CombatDummy.generated.h"

class UWorld;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UArrowComponent;

class AAIController;
class UHealthComponent;
class UNpcPatrolComponent;
class ABulletActor;

UCLASS()
class SANDBOX_API ACombatDummy
    : public ACharacter
    , public IDeathHandler
    , public IGenericTeamAgentInterface
    , public ISandboxMobInterface
    , public IDescribable {
    GENERATED_BODY()
  public:
    ACombatDummy();

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // ISandboxMobInterface
    virtual float get_acceptable_radius() const override { return 0.f; }
    virtual float get_attack_acceptable_radius() const override { return 0.f; }
    virtual EAIState get_default_ai_state() const override { return EAIState::Idle; }

    // IDescribable
    virtual FText get_description() const override { return FText::FromName(TEXT("Combat Dummy")); }
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    // IDeathHandler
    virtual void handle_death() override { Destroy(); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
    UStaticMeshComponent* body_mesh{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UHealthComponent* health{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UNpcPatrolComponent* patrol_state{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    ETeamID team_id{ETeamID::Neutral};
};
