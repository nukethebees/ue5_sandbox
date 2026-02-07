#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipSpeedWidget.generated.h"

UCLASS()
class SANDBOX_API UShipSpeedWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_speed(float speed);
};
