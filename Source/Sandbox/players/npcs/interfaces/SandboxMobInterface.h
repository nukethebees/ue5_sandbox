#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTree.h"
#include "UObject/Interface.h"

#include "Sandbox/players/npcs/enums/AIState.h"

#include "SandboxMobInterface.generated.h"

UINTERFACE(MinimalAPI)
class USandboxMobInterface : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API ISandboxMobInterface {
    GENERATED_BODY()
  public:
    virtual UBehaviorTree* get_behaviour_tree_asset() const { return nullptr; }
    virtual float get_acceptable_radius() const { return 100.0f; }
    virtual float get_attack_acceptable_radius() const { return 100.0f; }
    virtual EAIState get_default_ai_state() const { return EAIState::RandomlyMove; }
};
