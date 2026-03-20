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
void UShipHudWidget::set_health(FShipHealth value) {
    RETURN_IF_NULLPTR(health_widget);
    health_widget->set_health(value);
}
void UShipHudWidget::set_energy(float value) {
    RETURN_IF_NULLPTR(energy_widget);
    energy_widget->set_energy(value);
}
void UShipHudWidget::set_points(int32 value) {
    RETURN_IF_NULLPTR(points_widget);
    points_widget->set_points(value);
}
void UShipHudWidget::set_bombs(int32 value) {
    RETURN_IF_NULLPTR(bombs_widget);
    bombs_widget->set_count(value);
}
void UShipHudWidget::set_gold_rings(int32 value) {
    RETURN_IF_NULLPTR(gold_rings_widget);
    gold_rings_widget->set_count(value);
}
void UShipHudWidget::set_lives(int32 value) {
    RETURN_IF_NULLPTR(lives_widget);
    lives_widget->set_value(value);
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

void UShipHudWidget::set_lock_on_widget_visibility(bool visible) {
    RETURN_IF_NULLPTR(lock_on_widget);

    lock_on_widget->SetVisibility(visible ? ESlateVisibility::Visible
                                          : ESlateVisibility::Collapsed);
}
void UShipHudWidget::set_lock_on_widget_position(FVector2d pos) {
    RETURN_IF_NULLPTR(lock_on_widget);
    TRY_INIT_PTR(slot, Cast<UCanvasPanelSlot>(lock_on_widget->Slot));
    slot->SetPosition(pos);
}
