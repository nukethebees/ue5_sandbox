#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "StatsMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UStatsMenuWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
};
