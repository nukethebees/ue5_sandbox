#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipGoldRingCountWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UShipGoldRingCountWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_count(int32 count);
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
