#pragma once

#include "Commandlets/Commandlet.h"

#include "EntityOverlayBenchmarkCommandlet.generated.h"

UCLASS()
class UEntityOverlayBenchmarkCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UEntityOverlayBenchmarkCommandlet();
    int32 Main(FString const& params) override;
};
