#pragma once

#include "Commandlets/Commandlet.h"

#include "Radar3DBenchmarkCommandlet.generated.h"

UCLASS()
class URadar3DBenchmarkCommandlet : public UCommandlet {
    GENERATED_BODY()
  public:
    URadar3DBenchmarkCommandlet();

    int32 Main(FString const& params) override;
};
