#pragma once

#include "Commandlets/Commandlet.h"

#include "SandboxNiagaraInspectCommandlet.generated.h"

UCLASS()
class USandboxNiagaraInspectCommandlet : public UCommandlet {
    GENERATED_BODY()

  public:
    USandboxNiagaraInspectCommandlet();

    int32 Main(FString const& parameters) override;
};
