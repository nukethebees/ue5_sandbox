#pragma once

#include "Commandlets/Commandlet.h"

#include "VolumeHeatmap3DBenchmarkCommandlet.generated.h"

UCLASS()
class UVolumeHeatmap3DBenchmarkCommandlet : public UCommandlet {
    GENERATED_BODY()
  public:
    UVolumeHeatmap3DBenchmarkCommandlet();
    int32 Main(FString const& params) override;
};
