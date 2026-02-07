#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipHealthWidget.generated.h"

UCLASS()
class SANDBOX_API UShipHealthWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_health(float health);
};
