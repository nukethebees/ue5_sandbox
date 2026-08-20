#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "LogsMenuWidget.generated.h"

UCLASS()
class SHOOTERGAME_API ULogsMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void on_widget_selected();
  protected:
    void NativeConstruct() override;
};
