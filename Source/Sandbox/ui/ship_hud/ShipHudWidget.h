#pragma once

#include "Sandbox/health/ShipHealth.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include <span>

#include "ShipHudWidget.generated.h"

class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;

class UShipSpeedWidget;
class UShipHealthWidget;
class UShipThrusterEnergyWidget;
class UShipPointsWidget;
class UShipBombCountWidget;
class UShipGoldRingCountWidget;
class UPlayerLivesWidget;

class UDebugGraphWidget;

UCLASS()
class SANDBOX_API UShipHudWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_speed(float value);
    void set_health(FShipHealth value);
    void set_energy(float value);
    void set_points(int32 value);
    void set_bombs(int32 value);
    void set_gold_rings(int32 value);
    void set_lives(int32 value);
    void set_crosshair_positions(FVector2d near, FVector2d far);
    void set_crosshair_colours(FLinearColor near, FLinearColor far);

    void set_lock_on_widget_visibility(bool visible);
    void set_lock_on_widget_position(FVector2d pos);

#if WITH_EDITOR
    void update_sampled_speed(std::span<FVector2d> samples, int32 oldest_index);
#endif
  protected:
    void NativeConstruct() override;

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
    UPROPERTY(meta = (BindWidget))
    UPlayerLivesWidget* lives_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* far_crosshair_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UImage* near_crosshair_widget{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    UMaterialInterface* crosshair_material{nullptr};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UMaterialInstanceDynamic* near_crosshair_material_instance{nullptr};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UMaterialInstanceDynamic* far_crosshair_material_instance{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* lock_on_widget{nullptr};
#if WITH_EDITORONLY_DATA
    UPROPERTY(meta = (BindWidget))
    UDebugGraphWidget* speed_graph{nullptr};
#endif
};
