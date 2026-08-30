#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateS7LabAssetsCommandlet.generated.h"

UCLASS()
class UGenerateS7LabAssetsCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateS7LabAssetsCommandlet();

    int32 Main(FString const& params) override;
};
