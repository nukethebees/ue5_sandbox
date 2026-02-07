#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipBombCountWidget.generated.h"

UCLASS()
class SANDBOX_API UShipBombCountWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_count(int32 count);
};
