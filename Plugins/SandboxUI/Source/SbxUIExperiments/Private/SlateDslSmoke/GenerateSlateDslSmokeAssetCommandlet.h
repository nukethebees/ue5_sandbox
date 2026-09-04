#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateSlateDslSmokeAssetCommandlet.generated.h"

UCLASS()
class UGenerateSlateDslSmokeAssetCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateSlateDslSmokeAssetCommandlet();

    int32 Main(FString const& parameters) override;
};
