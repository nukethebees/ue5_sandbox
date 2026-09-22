#pragma once

#include "Commandlets/Commandlet.h"

#include "MaterialSynthCommandlet.generated.h"

UCLASS()
class UMaterialSynthCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UMaterialSynthCommandlet();
    int32 Main(FString const& parameters) override;
};
