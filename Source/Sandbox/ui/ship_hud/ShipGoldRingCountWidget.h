#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipGoldRingCountWidget.generated.h"

UCLASS()
class SANDBOX_API UShipGoldRingCountWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_count(int32 count);
};
