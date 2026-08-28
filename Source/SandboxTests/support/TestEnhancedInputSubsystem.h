#pragma once

#include <EnhancedInputSubsystemInterface.h>

#include <CoreMinimal.h>

#include "TestEnhancedInputSubsystem.generated.h"

class UEnhancedPlayerInput;

UCLASS()
class USandboxTestEnhancedInputSubsystem
    : public UObject
    , public IEnhancedInputSubsystemInterface {
    GENERATED_BODY()
  public:
    void initialise();

    auto GetPlayerInput() const -> UEnhancedPlayerInput* override { return player_input; }
  protected:
    auto GetContinuouslyInjectedInputs()
        -> TMap<TObjectPtr<UInputAction const>, FInjectedInput>& override {
        return continuously_injected_inputs;
    }
  private:
    UPROPERTY(Transient)
    TObjectPtr<UEnhancedPlayerInput> player_input{nullptr};

    UPROPERTY(Transient)
    TMap<TObjectPtr<UInputAction const>, FInjectedInput> continuously_injected_inputs;
};
