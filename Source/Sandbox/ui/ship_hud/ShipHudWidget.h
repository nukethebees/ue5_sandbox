#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipHudWidget.generated.h"

class UShipSpeedWidget;
class UShipHealthWidget;
class UShipThrusterEnergyWidget;
class UShipPointsWidget;
class UShipBombCountWidget;
class UShipGoldRingCountWidget;

UCLASS()
class SANDBOX_API UShipHudWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_speed(float value);
    void set_health(float value);
    void set_energy(float value);
    void set_points(int32 value);
    void set_bombs(int32 value);
    void set_gold_rings(int32 value);
  protected:
    UPROPERTY(meta = (BindWidget))
    UShipSpeedWidget* speed_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipHealthWidget* health_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipThrusterEnergyWidget* energy_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipPointsWidget* points_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipBombCountWidget* bombs_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipGoldRingCountWidget* gold_rings_widget{nullptr};
};
