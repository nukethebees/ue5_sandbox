#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ResearchMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UResearchMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void on_widget_selected();
  protected:
    virtual void NativeConstruct() override;
};
