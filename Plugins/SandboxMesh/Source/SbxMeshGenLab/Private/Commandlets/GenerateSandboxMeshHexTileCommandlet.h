#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshHexTileCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshHexTileCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshHexTileCommandlet();

    int32 Main(FString const& parameters) override;
};
