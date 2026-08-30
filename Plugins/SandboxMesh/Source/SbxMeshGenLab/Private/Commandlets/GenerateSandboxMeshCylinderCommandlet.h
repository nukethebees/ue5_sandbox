#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshCylinderCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshCylinderCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshCylinderCommandlet();

    int32 Main(FString const& parameters) override;
};
