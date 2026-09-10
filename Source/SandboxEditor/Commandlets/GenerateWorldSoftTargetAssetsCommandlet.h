#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateWorldSoftTargetAssetsCommandlet.generated.h"

UCLASS()
class UGenerateWorldSoftTargetAssetsCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateWorldSoftTargetAssetsCommandlet();
    int32 Main(FString const& params) override;
};
