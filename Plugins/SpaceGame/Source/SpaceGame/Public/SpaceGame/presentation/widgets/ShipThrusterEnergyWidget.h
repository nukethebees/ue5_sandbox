#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include "ShipThrusterEnergyWidget.generated.h"

class UProgressBar;

UCLASS()
class SPACEGAME_API UShipThrusterEnergyWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_energy(float energy);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
  protected:
    UPROPERTY(meta = (BindWidget))
    UProgressBar* energy_bar{nullptr};
};
