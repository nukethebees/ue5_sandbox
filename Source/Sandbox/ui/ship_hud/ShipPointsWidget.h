#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipPointsWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UShipPointsWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_points(int32 points);
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
