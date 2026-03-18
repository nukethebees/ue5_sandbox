#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

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

#include "Sandbox/utilities/macros/null_checks.hpp"

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
void UShipHudWidget::set_crosshairs(FVector2d near, FVector2d far) {
    RETURN_IF_NULLPTR(far_crosshair_widget);
    RETURN_IF_NULLPTR(near_crosshair_widget);

    TRY_INIT_PTR(far_slot, Cast<UCanvasPanelSlot>(far_crosshair_widget->Slot));
    TRY_INIT_PTR(near_slot, Cast<UCanvasPanelSlot>(near_crosshair_widget->Slot));

    far_slot->SetPosition(far);
    near_slot->SetPosition(near);
}
