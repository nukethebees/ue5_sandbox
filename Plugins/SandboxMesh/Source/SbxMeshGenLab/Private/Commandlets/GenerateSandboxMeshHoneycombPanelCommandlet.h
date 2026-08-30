#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshHoneycombPanelCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshHoneycombPanelCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshHoneycombPanelCommandlet();

    int32 Main(FString const& parameters) override;
};
