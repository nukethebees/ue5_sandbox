#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#include "Sandbox/health/DeathHandler.h"
#include "Sandbox/interaction/Describable.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/players/npcs/enums/AIState.h"
#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"

#include "CombatDummy.generated.h"

class AAIController;
class UStaticMeshComponent;

class UHealthComponent;
class UNpcPatrolComponent;

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

    auto get_default_ai_state() const { return default_ai_state; }

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // IDescribable
    virtual FText get_description() const override { return FText::FromName(TEXT("Combat Dummy")); }
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    // IDeathHandler
    virtual void handle_death() override { Destroy(); }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
    UStaticMeshComponent* body_mesh{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UNpcPatrolComponent* patrol_state{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    TSubclassOf<AAIController> controller_class{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UHealthComponent* health{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    ETeamID team_id{ETeamID::Neutral};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    EAIState default_ai_state{EAIState::Idle};
};
