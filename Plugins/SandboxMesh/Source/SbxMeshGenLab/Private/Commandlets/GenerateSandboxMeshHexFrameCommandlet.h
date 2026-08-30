#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshHexFrameCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshHexFrameCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshHexFrameCommandlet();

    int32 Main(FString const& parameters) override;
};
