#pragma once

#include "Commandlets/Commandlet.h"

#include "ImportGameAudioCommandlet.generated.h"

UCLASS()
class UImportGameAudioCommandlet : public UCommandlet {
    GENERATED_BODY()
  public:
    UImportGameAudioCommandlet();
    int32 Main(FString const& params) override;
};
