#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "PlayerLivesWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UPlayerLivesWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_value(int32 value);
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
