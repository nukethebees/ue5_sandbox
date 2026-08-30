#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshBoxCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshBoxCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshBoxCommandlet();

    int32 Main(FString const& parameters) override;
};
