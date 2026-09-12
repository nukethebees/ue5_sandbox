#pragma once

#include "Commandlets/Commandlet.h"

#include "MergeSaveProfilesCommandlet.generated.h"

UCLASS()
class UMergeSaveProfilesCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UMergeSaveProfilesCommandlet();
    int32 Main(FString const& parameters) override;
};
