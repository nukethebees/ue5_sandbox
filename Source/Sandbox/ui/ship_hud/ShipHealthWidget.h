#pragma once

#include "Sandbox/health/ShipHealth.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipHealthWidget.generated.h"

class UProgressBar;

UCLASS()
class SANDBOX_API UShipHealthWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_health(FShipHealth health);
  protected:
    UPROPERTY(meta = (BindWidget))
    UProgressBar* health_bar{nullptr};
};
