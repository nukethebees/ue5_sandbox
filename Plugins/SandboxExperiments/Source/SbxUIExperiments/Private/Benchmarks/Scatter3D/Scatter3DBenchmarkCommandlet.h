#pragma once

#include "Commandlets/Commandlet.h"

#include "Scatter3DBenchmarkCommandlet.generated.h"

UCLASS()
class UScatter3DBenchmarkCommandlet : public UCommandlet {
    GENERATED_BODY()
  public:
    UScatter3DBenchmarkCommandlet();

    int32 Main(FString const& params) override;
};
