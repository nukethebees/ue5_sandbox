#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/ship_hud/PlayerLivesWidget.h"
#include "Sandbox/ui/ship_hud/ShipBombCountWidget.h"
#include "Sandbox/ui/ship_hud/ShipGoldRingCountWidget.h"
#include "Sandbox/ui/ship_hud/ShipHealthWidget.h"
#include "Sandbox/ui/ship_hud/ShipPointsWidget.h"
#include "Sandbox/ui/ship_hud/ShipSpeedWidget.h"
#include "Sandbox/ui/ship_hud/ShipThrusterEnergyWidget.h"
#include "Sandbox/ui/widgets/DebugGraphWidget.h"
#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipHudWidget::NativeConstruct() {
    Super::NativeConstruct();

    RETURN_IF_NULLPTR(crosshair_material);
    RETURN_IF_NULLPTR(far_crosshair_widget);
    RETURN_IF_NULLPTR(near_crosshair_widget);

    near_crosshair_material_instance = UMaterialInstanceDynamic::Create(crosshair_material, this);
    far_crosshair_material_instance = UMaterialInstanceDynamic::Create(crosshair_material, this);

    RETURN_IF_NULLPTR(near_crosshair_material_instance);
    RETURN_IF_NULLPTR(far_crosshair_material_instance);

    near_crosshair_widget->SetBrushFromMaterial(near_crosshair_material_instance);
    far_crosshair_widget->SetBrushFromMaterial(far_crosshair_material_instance);
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

void UShipHudWidget::set_bombs(int32 value) {
    RETURN_IF_NULLPTR(bombs_widget);
    bombs_widget->set_count(value);
}
void UShipHudWidget::set_bombs_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(bombs_widget, new_visibility);
}

void UShipHudWidget::set_gold_rings(int32 value) {
    RETURN_IF_NULLPTR(gold_rings_widget);
    gold_rings_widget->set_count(value);
}
void UShipHudWidget::set_gold_rings_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(gold_rings_widget, new_visibility);
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
    check(IsValid(stopwatch_widget));
    mission_status_widget->update(value);
}
void UShipHudWidget::set_mission_status_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(mission_status_widget, new_visibility);
}

void UShipHudWidget::set_fire_rate(FStringView const value) {
    check(IsValid(fire_rate_widget));
    fire_rate_widget->update(value);
}
void UShipHudWidget::set_fire_rate_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(fire_rate_widget, new_visibility);
}

void UShipHudWidget::set_lives(int32 value) {
    RETURN_IF_NULLPTR(lives_widget);
    lives_widget->set_value(value);
}
void UShipHudWidget::set_lives_widget_visibility(ESlateVisibility const new_visibility) {
    set_widget_visibility_checked(lives_widget, new_visibility);
}

void UShipHudWidget::update_sampled_speed(std::span<FVector2d> samples, int32 oldest_index) {
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
