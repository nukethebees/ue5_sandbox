#pragma once

#include "Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h"
#include "Sandbox/batch_game/TestMissionState.h"
#include "Sandbox/batch_game/TestTeamVisualData.h"
#include "Sandbox/health/ShipHealth.h"
#include "Sandbox/ui/HudCrosshairDistances.h"
#include "Sandbox/ui/ship_hud/ShipHudKillData.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipHudWidget.generated.h"

class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;

class UShipSpeedWidget;
class UShipHealthWidget;
class UShipThrusterEnergyWidget;
class UShipPointsWidget;

class UValueWidget;
class UVector2DWidget;
class UDebugGraphWidget;
class UEntityCountTableWidget;
class UMissionStatusWidget;
class UTeamKillMatrixWidget;
class UTopKillersWidget;
namespace ml::hud_manager {
struct FMissionDataCache;
}

UCLASS()
class SANDBOX_API UShipHudWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_speed(float value);
    void set_speed_widget_visibility(ESlateVisibility const new_visibility);

    void set_health(FShipHealth value);
    void set_health_widget_visibility(ESlateVisibility const new_visibility);

    void set_energy(float value);
    void set_energy_widget_visibility(ESlateVisibility const new_visibility);

    void set_points(int32 value);
    void set_points_widget_visibility(ESlateVisibility const new_visibility);

    void set_stopwatch_time(float const time_s);
    void set_stopwatch_widget_visibility(ESlateVisibility const new_visibility);

    void set_mission_status(FStringView const mission_status);
    void set_mission_status_widget_visibility(ESlateVisibility const new_visibility);

    void set_fire_rate(FStringView const mission_status);
    void set_fire_rate_visibility(ESlateVisibility const new_visibility);

    void set_crosshair_positions(FVector2d near, FVector2d far);
    void set_crosshair_colours(FLinearColor near, FLinearColor far);
    void set_crosshair_widget_visibility(ESlateVisibility const new_visibility);
    void set_crosshair_distances(FHudCrosshairDistances const& value) {
        crosshair_distances = value;
    }
    auto get_crosshair_distances() const noexcept -> FHudCrosshairDistances const& {
        return crosshair_distances;
    }

    void set_lock_on_widget_position(FVector2d pos);
    void set_lock_on_widget_visibility(bool const new_visibility);
    void set_lock_on_widget_visibility(ESlateVisibility const new_visibility);

    void set_target_speed(float value);

    void set_selected_imc(FStringView value);

    void set_turning(FVector2D value);
    void set_moving(FVector2D value);
    void set_desired_velocity_scale(FVector2D value);
    void set_ship_velocity(FVector value);
    void set_target_velocity(FVector value);
    void set_control_mode(FStringView value);
    void set_flight_mode(FStringView value);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
    void set_entity_counts(ATestEntityRegistry::EntityCounts const& counts);
    void set_entity_colours(UTestTeamVisualData::FColourArray const& colours);
    void set_top_killers(ml::ship_hud::FTopKillerEntries const& entries);
    void set_team_kill_matrix(ml::ship_hud::FTeamKillMatrix const& matrix);
    void set_mission_data(ml::hud_manager::FMissionDataCache const& data);
    void set_mission_state(ETestMissionState const new_state);
    void set_mission_time(float const mission_time);
    void set_mission_time_remaining(float const time_remaining);
    void set_mission_enemies_remaining(int32 const enemies_remaining);

#if WITH_EDITOR
    void update_sampled_speed(TConstArrayView<FVector2d> samples, int32 oldest_index);
#endif
  protected:
    void NativePreConstruct() override;
    void NativeConstruct() override;

    void set_common_widget_properties();
    void set_widget_visibility_checked(UWidget* const widget,
                                       ESlateVisibility const new_visibility);

    UPROPERTY(meta = (BindWidget))
    UShipSpeedWidget* speed_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipHealthWidget* health_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipThrusterEnergyWidget* energy_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UShipPointsWidget* points_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* stopwatch_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* mission_status_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* fire_rate_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* target_speed_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* selected_imc_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVector2DWidget* turning_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVector2DWidget* moving_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVector2DWidget* desired_velocity_scale_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* ship_velocity_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* target_velocity_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* control_mode_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UValueWidget* flight_mode_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UEntityCountTableWidget* entity_count_table{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTopKillersWidget* top_killers_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTeamKillMatrixWidget* team_kill_matrix_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMissionStatusWidget* mission_status_panel{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};

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

    FHudCrosshairDistances crosshair_distances{};

    UPROPERTY(meta = (BindWidget))
    UImage* lock_on_widget{nullptr};
#if WITH_EDITORONLY_DATA
    UPROPERTY(meta = (BindWidget))
    UDebugGraphWidget* speed_graph{nullptr};
#endif
};
