#pragma once

#include "Commandlets/Commandlet.h"

#include "ImportMenuAmbienceCommandlet.generated.h"

UCLASS()
class UImportMenuAmbienceCommandlet : public UCommandlet {
    GENERATED_BODY()
  public:
    UImportMenuAmbienceCommandlet();
    int32 Main(FString const& params) override;
};
