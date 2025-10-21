#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "LogsMenuWidget.generated.h"

UCLASS()
class SANDBOX_API ULogsMenuWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
};
