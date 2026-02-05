#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ObjectivesMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UObjectivesMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void on_widget_selected();
  protected:
    virtual void NativeConstruct() override;
};
