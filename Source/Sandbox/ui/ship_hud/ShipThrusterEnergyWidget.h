#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipThrusterEnergyWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UShipThrusterEnergyWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_energy(float energy);
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
