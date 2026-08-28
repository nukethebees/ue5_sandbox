#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSandboxImagesCommandlet.generated.h"

UCLASS()
class UGenerateSandboxImagesCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSandboxImagesCommandlet();

    int32 Main(FString const& parameters) override;
};
