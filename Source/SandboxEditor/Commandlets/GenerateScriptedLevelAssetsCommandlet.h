#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateScriptedLevelAssetsCommandlet.generated.h"

UCLASS()
class UGenerateScriptedLevelAssetsCommandlet : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateScriptedLevelAssetsCommandlet();
    int32 Main(FString const& params) override;
};
