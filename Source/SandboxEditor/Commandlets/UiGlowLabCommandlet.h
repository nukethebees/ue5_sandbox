#pragma once

#include "Commandlets/Commandlet.h"

#include "UiGlowLabCommandlet.generated.h"

UCLASS()
class UUiGlowLabCommandlet final : public UCommandlet {
    GENERATED_BODY()
  public:
    UUiGlowLabCommandlet();
    int32 Main(FString const& params) override;
};
