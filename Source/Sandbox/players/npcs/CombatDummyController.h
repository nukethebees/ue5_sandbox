#pragma once

#include "CoreMinimal.h"
#include "AIController.h"

#include "Sandbox/players/npcs/AIState.h"
#include "Sandbox/players/npcs/npc_delegates.h"

#include "Sandbox/players/npcs/blackboard_utils.hpp"

#include "CombatDummyController.generated.h"

class UWorld;
class APawn;

class UBehaviorTree;

UCLASS()
class SANDBOX_API ACombatDummyController : public AAIController {
    GENERATED_BODY()
  public:
    ACombatDummyController();
  protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    void set_bb_value(FName const& name, auto const& value) {
        check(Blackboard);
        ml::set_bb_value(*Blackboard, name, value);
    }
    void set_ai_state(EAIState state);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    UBehaviorTree* behaviour_tree_asset{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    EAIState ai_state{EAIState::Idle};
};
