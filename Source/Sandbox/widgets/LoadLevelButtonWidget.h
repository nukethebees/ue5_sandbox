#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"

#include "LoadLevelButtonWidget.generated.h"

UCLASS()
class SANDBOX_API ULoadLevelButtonWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(meta = (BindWidget))
    UButton* load_level_button;
  protected:
    virtual void NativeConstruct() override;
  private:
    void load_level();
};
