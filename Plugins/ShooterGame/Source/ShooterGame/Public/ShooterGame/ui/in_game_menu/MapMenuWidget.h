#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MapMenuWidget.generated.h"

UCLASS()
class SHOOTERGAME_API UMapMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void on_widget_selected();
  protected:
    void NativeConstruct() override;
};
