#pragma once

#include <NiagaraComponent.h>

#include "TestNiagaraComponent.generated.h"

UCLASS()
class UTestNiagaraComponent : public UNiagaraComponent {
    GENERATED_BODY()
  public:
    void Activate(bool = false) override {
        ++activation_count;
        active = true;
    }
    void Deactivate() override { active = false; }

    uint64 activation_count{};
    bool active{};
};
