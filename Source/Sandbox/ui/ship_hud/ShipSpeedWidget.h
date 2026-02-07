#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipSpeedWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UShipSpeedWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_speed(float speed);
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
