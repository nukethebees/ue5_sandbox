#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateLuaLabAssetsCommandlet.generated.h"

UCLASS()
class UGenerateLuaLabAssetsCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UGenerateLuaLabAssetsCommandlet();

    int32 Main(FString const& params) override;
};
