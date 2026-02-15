#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTree.h"
#include "UObject/Interface.h"

#include "Sandbox/players/AIState.h"

#include "SandboxMobInterface.generated.h"

UINTERFACE(MinimalAPI)
class USandboxMobInterface : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API ISandboxMobInterface {
    GENERATED_BODY()
  public:
    virtual float get_acceptable_radius() const { return 100.0f; }
    virtual float get_attack_acceptable_radius() const { return 100.0f; }
    // AI state
    virtual EAIState get_default_ai_state() const { return EAIState::RandomlyMove; }
};
