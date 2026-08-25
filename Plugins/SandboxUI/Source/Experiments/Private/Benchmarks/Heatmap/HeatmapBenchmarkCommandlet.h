#pragma once

#include "Commandlets/Commandlet.h"

#include "HeatmapBenchmarkCommandlet.generated.h"

UCLASS()
class UHeatmapBenchmarkCommandlet : public UCommandlet {
    GENERATED_BODY()
  public:
    UHeatmapBenchmarkCommandlet();

    int32 Main(FString const& params) override;
};
