#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipHealthWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UShipHealthWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_health(float health);
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
