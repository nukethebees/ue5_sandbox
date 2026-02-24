#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

#include "Sandbox/ui/ship_hud/PlayerLivesWidget.h"
#include "Sandbox/ui/ship_hud/ShipBombCountWidget.h"
#include "Sandbox/ui/ship_hud/ShipGoldRingCountWidget.h"
#include "Sandbox/ui/ship_hud/ShipHealthWidget.h"
#include "Sandbox/ui/ship_hud/ShipPointsWidget.h"
#include "Sandbox/ui/ship_hud/ShipSpeedWidget.h"
#include "Sandbox/ui/ship_hud/ShipThrusterEnergyWidget.h"
#include "Sandbox/ui/widgets/DebugGraphWidget.h"

void UShipHudWidget::set_speed(float value) {
    if (!speed_widget) {
        return;
    }
    speed_widget->set_speed(value);
}
void UShipHudWidget::set_health(FShipHealth value) {
    if (!health_widget) {
        return;
    }
    health_widget->set_health(value);
}
void UShipHudWidget::set_energy(float value) {
    if (!energy_widget) {
        return;
    }
    energy_widget->set_energy(value);
}
void UShipHudWidget::set_points(int32 value) {
    if (!points_widget) {
        return;
    }
    points_widget->set_points(value);
}
void UShipHudWidget::set_bombs(int32 value) {
    if (!bombs_widget) {
        return;
    }
    bombs_widget->set_count(value);
}
void UShipHudWidget::set_gold_rings(int32 value) {
    if (!gold_rings_widget) {
        return;
    }
    gold_rings_widget->set_count(value);
}
void UShipHudWidget::set_lives(int32 value) {
    if (!lives_widget) {
        return;
    }
    lives_widget->set_value(value);
}
void UShipHudWidget::update_sampled_speed(std::span<FVector2d> samples, int32 oldest_index) {
    if (!speed_graph) {
        return;
    }
    speed_graph->set_samples(samples, oldest_index);
}
