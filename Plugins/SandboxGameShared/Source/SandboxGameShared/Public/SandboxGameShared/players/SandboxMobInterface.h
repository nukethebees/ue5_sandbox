#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "SandboxGameShared/players/AIState.h"

#include "SandboxMobInterface.generated.h"

UINTERFACE(MinimalAPI)
class USandboxMobInterface : public UInterface {
    GENERATED_BODY()
};

class SANDBOXGAMESHARED_API ISandboxMobInterface {
    GENERATED_BODY()
  public:
    virtual float get_acceptable_radius() const { return 100.0f; }
    virtual float get_attack_acceptable_radius() const { return 100.0f; }
    // AI state
    virtual EAIState get_default_ai_state() const { return EAIState::RandomlyMove; }
};
