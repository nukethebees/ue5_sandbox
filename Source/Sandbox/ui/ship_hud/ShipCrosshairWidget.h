#pragma once

#include "Blueprint/UserWidget.h"

#include "ShipCrosshairWidget.generated.h"

class UImage;

UCLASS()
class SANDBOX_API UShipCrosshairWidget : public UUserWidget {
  public:
    GENERATED_BODY()
  protected:
    UPROPERTY(meta = (BindWidget))
    UImage* near_crosshair{nullptr};
    UPROPERTY(meta = (BindWidget))
    UImage* far_crosshair{nullptr};
};
