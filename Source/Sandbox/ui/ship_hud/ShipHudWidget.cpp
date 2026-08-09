#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

#include <Sandbox/ui/HUDManager.h>

#include "Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/ship_hud/EntityCountTableWidget.h"
#include "Sandbox/ui/ship_hud/MissionStatusWidget.h"
#include "Sandbox/ui/ship_hud/ShipHealthWidget.h"
#include "Sandbox/ui/ship_hud/ShipPointsWidget.h"
#include "Sandbox/ui/ship_hud/ShipSpeedWidget.h"
#include "Sandbox/ui/ship_hud/ShipThrusterEnergyWidget.h"
#include "Sandbox/ui/widgets/DebugGraphWidget.h"
#include "Sandbox/ui/widgets/ValueWidget.h"
#include "Sandbox/ui/widgets/Vector2DWidget.h"

#include <Blueprint/WidgetTree.h>
#include <Components/CanvasPanelSlot.h>
#include <Components/Image.h>
#include <Components/PanelWidget.h>
#include <Components/Widget.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Materials/MaterialInterface.h>

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipHudWidget::NativeConstruct() {
    Super::NativeConstruct();
    set_common_widget_properties();

    RETURN_IF_NULLPTR(crosshair_material);
    RETURN_IF_NULLPTR(far_crosshair_widget);
    RETURN_IF_NULLPTR(near_crosshair_widget);
    RETURN_IF_NULLPTR(entity_count_table);

    near_crosshair_material_instance = UMaterialInstanceDynamic::Create(crosshair_material, this);
    far_crosshair_material_instance = UMaterialInstanceDynamic::Create(crosshair_material, this);

    RETURN_IF_NULLPTR(near_crosshair_material_instance);
    RETURN_IF_NULLPTR(far_crosshair_material_instance);

    near_crosshair_widget->SetBrushFromMaterial(near_crosshair_material_instance);
    far_crosshair_widget->SetBrushFromMaterial(far_crosshair_material_instance);
}

void UShipHudWidget::NativePreConstruct() {
    Super::NativePreConstruct();
    set_common_widget_properties();
}

void UShipHudWidget::set_common_widget_properties() {
    speed_widget->set_font_size(font_size);
    health_widget->set_font_size(font_size);
    points_widget->set_font_size(font_size);

    stopwatch_widget->set_font_size(font_size);
    mission_status_widget->set_font_size(font_size);
    fire_rate_widget->set_font_size(font_size);
    target_speed_widget->set_font_size(font_size);
    selected_imc_widget->set_font_size(font_size);
    turning_widget->set_font_size(font_size);
    moving_widget->set_font_size(font_size);
    desired_velocity_scale_widget->set_font_size(font_size);
    ship_velocity_widget->set_font_size(font_size);
    target_velocity_widget->set_font_size(font_size);
    control_mode_widget->set_font_size(font_size);
    flight_mode_widget->set_font_size(font_size);

    entity_count_table->set_font_size(font_size);
    mission_status_panel->set_font_size(font_size);
}

void UShipHudWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    set_common_widget_properties();
}

void UShipHudWidget::set_speed(float value) {
    RETURN_IF_NULLPTR(speed_widget);
    speed_widget->set_speed(value);
}
void UShipHudWidget::set_speed_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(speed_widget, new_visibility);
}

void UShipHudWidget::set_health(FShipHealth value) {
    RETURN_IF_NULLPTR(health_widget);
    health_widget->set_health(value);
}
void UShipHudWidget::set_health_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(health_widget, new_visibility);
}

void UShipHudWidget::set_energy(float value) {
    RETURN_IF_NULLPTR(energy_widget);
    energy_widget->set_energy(value);
}
void UShipHudWidget::set_energy_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(energy_widget, new_visibility);
}

void UShipHudWidget::set_points(int32 value) {
    RETURN_IF_NULLPTR(points_widget);
    points_widget->set_points(value);
}
void UShipHudWidget::set_points_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(points_widget, new_visibility);
}

void UShipHudWidget::set_stopwatch_time(float const time_s) {
    check(IsValid(stopwatch_widget));

    auto const total_seconds{static_cast<int32>(time_s)};
    auto const minutes{total_seconds / 60};
    auto const seconds{total_seconds % 60};

    auto const frac_part{time_s - FMath::Floor(time_s)};
    auto const centiseconds{static_cast<int32>(frac_part * 100.f)};

    FNumberFormattingOptions options;
    options.MinimumIntegralDigits = 2;

    stopwatch_widget->update(options, minutes, seconds, centiseconds);
}
void UShipHudWidget::set_stopwatch_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(stopwatch_widget, new_visibility);
}

void UShipHudWidget::set_mission_status(FStringView const value) {
    check(IsValid(mission_status_widget));
    mission_status_widget->update(value);
}
void UShipHudWidget::set_mission_status_widget_visibility(ESlateVisibility const new_visibility) {
    if (mission_status_widget) {
        mission_status_widget->SetVisibility(new_visibility);
    }
    if (mission_status_panel) {
        mission_status_panel->SetVisibility(new_visibility);
    }
}

void UShipHudWidget::set_fire_rate(FStringView const value) {
    check(IsValid(fire_rate_widget));
    fire_rate_widget->update(value);
}
void UShipHudWidget::set_fire_rate_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(fire_rate_widget, new_visibility);
}

void UShipHudWidget::set_target_speed(float value) {
    check(IsValid(target_speed_widget));
    target_speed_widget->update(value);
}

void UShipHudWidget::set_selected_imc(FStringView value) {
    check(IsValid(selected_imc_widget));
    selected_imc_widget->update(value);
}

void UShipHudWidget::set_turning(FVector2D value) {
    check(IsValid(turning_widget));
    turning_widget->update(value);
}

void UShipHudWidget::set_moving(FVector2D value) {
    check(IsValid(moving_widget));
    moving_widget->update(value);
}

void UShipHudWidget::set_desired_velocity_scale(FVector2D value) {
    check(IsValid(desired_velocity_scale_widget));
    desired_velocity_scale_widget->update(value);
}

void UShipHudWidget::set_ship_velocity(FVector value) {
    check(IsValid(ship_velocity_widget));
    ship_velocity_widget->update(value.ToCompactString());
}

void UShipHudWidget::set_target_velocity(FVector value) {
    check(IsValid(target_velocity_widget));
    target_velocity_widget->update(value.ToCompactString(), value.Size());
}

void UShipHudWidget::set_control_mode(FStringView value) {
    check(IsValid(control_mode_widget));
    control_mode_widget->update(value);
}

void UShipHudWidget::set_flight_mode(FStringView value) {
    check(IsValid(flight_mode_widget));
    flight_mode_widget->update(value);
}

void UShipHudWidget::set_entity_counts(ATestEntityRegistry::EntityCounts const& counts) {
    if (entity_count_table) {
        entity_count_table->set_entity_counts(counts);
    }
}

void UShipHudWidget::set_entity_colours(UTestTeamVisualData::FColourArray const& colours) {
    if (entity_count_table) {
        entity_count_table->set_team_colours(colours);
    }
}

void UShipHudWidget::set_mission_data(ml::hud_manager::FMissionDataCache const& data) {
    if (mission_status_panel) {
        mission_status_panel->set_mission_data(data);
    }
}

void UShipHudWidget::set_mission_state(ETestMissionState const new_state) {
    if (mission_status_panel) {
        mission_status_panel->set_mission_state(new_state);
    }
}

void UShipHudWidget::set_mission_time(float const mission_time) {
    if (mission_status_panel) {
        mission_status_panel->set_mission_time(mission_time);
    }
}

void UShipHudWidget::set_mission_time_remaining(float const time_remaining) {
    if (mission_status_panel) {
        mission_status_panel->set_time_remaining(time_remaining);
    }
}

void UShipHudWidget::set_mission_enemies_remaining(int32 const enemies_remaining) {
    if (mission_status_panel) {
        mission_status_panel->set_enemies_remaining(enemies_remaining);
    }
}

void UShipHudWidget::update_sampled_speed(TConstArrayView<FVector2d> const samples,
                                          int32 const oldest_index) {
    RETURN_IF_NULLPTR(speed_graph);
    speed_graph->set_samples(samples, oldest_index);
}

void UShipHudWidget::set_crosshair_positions(FVector2d near, FVector2d far) {
    RETURN_IF_NULLPTR(far_crosshair_widget);
    RETURN_IF_NULLPTR(near_crosshair_widget);

    TRY_INIT_PTR(far_slot, Cast<UCanvasPanelSlot>(far_crosshair_widget->Slot));
    TRY_INIT_PTR(near_slot, Cast<UCanvasPanelSlot>(near_crosshair_widget->Slot));

    far_slot->SetPosition(far);
    near_slot->SetPosition(near);
}
void UShipHudWidget::set_crosshair_colours(FLinearColor near, FLinearColor far) {
    RETURN_IF_NULLPTR(near_crosshair_material_instance);
    RETURN_IF_NULLPTR(far_crosshair_material_instance);

    FName const name{TEXT("colour")};

    UE_LOG(LogSandboxUI, Verbose, TEXT("Setting colour parameters."));

    near_crosshair_material_instance->SetVectorParameterValue(name, near);
    far_crosshair_material_instance->SetVectorParameterValue(name, far);
}
void UShipHudWidget::set_crosshair_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(far_crosshair_widget, new_visibility);
    set_widget_visibility_checked(near_crosshair_widget, new_visibility);
}

void UShipHudWidget::set_lock_on_widget_visibility(bool const visible) {
    RETURN_IF_NULLPTR(lock_on_widget);

    lock_on_widget->SetVisibility(visible ? ESlateVisibility::Visible
                                          : ESlateVisibility::Collapsed);
}
void UShipHudWidget::set_lock_on_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(lock_on_widget, new_visibility);
}
void UShipHudWidget::set_lock_on_widget_position(FVector2d pos) {
    RETURN_IF_NULLPTR(lock_on_widget);
    TRY_INIT_PTR(slot, Cast<UCanvasPanelSlot>(lock_on_widget->Slot));
    slot->SetPosition(pos);
}

void UShipHudWidget::set_widget_visibility_checked(UWidget* const widget,
                                                   ESlateVisibility const new_visibility) {
    check(IsValid(widget));
    widget->SetVisibility(new_visibility);
}
