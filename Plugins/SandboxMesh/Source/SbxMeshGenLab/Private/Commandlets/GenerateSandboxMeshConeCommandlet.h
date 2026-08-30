#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshConeCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshConeCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshConeCommandlet();

    int32 Main(FString const& parameters) override;
};
