#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshCubeCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshCubeCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshCubeCommandlet();

    int32 Main(FString const& parameters) override;
};
