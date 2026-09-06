#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshAssembliesCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshAssembliesCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshAssembliesCommandlet();

    int32 Main(FString const& parameters) override;
};
