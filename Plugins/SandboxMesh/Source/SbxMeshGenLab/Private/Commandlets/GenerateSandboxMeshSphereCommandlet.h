#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxMeshSphereCommandlet.generated.h"

UCLASS()
class UGenerateSandboxMeshSphereCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxMeshSphereCommandlet();

    int32 Main(FString const& parameters) override;
};
