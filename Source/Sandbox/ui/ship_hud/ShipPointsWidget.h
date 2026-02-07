#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipPointsWidget.generated.h"

UCLASS()
class SANDBOX_API UShipPointsWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_points(int32 points);
};
