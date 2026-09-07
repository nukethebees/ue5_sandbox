#include "SpaceGame/presentation/widgets/ShipHudWidget.h"

#include <SpaceGame/presentation/HUDManager.h>

#include "SandboxGameShared/ui/widgets/ValueWidget.h"
#include "SandboxUI/Radar/SRadarWidget.h"
#include "SpaceGame/entities/TestEntityRegistry.h"
#include "SpaceGame/presentation/widgets/DebugGraphWidget.h"
#include "SpaceGame/presentation/widgets/ForceStatusWidget.h"
#include "SpaceGame/presentation/widgets/MissionStatusWidget.h"
#include "SpaceGame/presentation/widgets/ShipHealthWidget.h"
#include "SpaceGame/presentation/widgets/ShipPointsWidget.h"
#include "SpaceGame/presentation/widgets/ShipSpeedWidget.h"
#include "SpaceGame/presentation/widgets/ShipThrusterEnergyWidget.h"
#include "SpaceGame/presentation/widgets/Vector2DWidget.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Blueprint/WidgetTree.h>
#include <Components/Border.h>
#include <Components/CanvasPanelSlot.h>
#include <Components/Image.h>
#include <Components/NativeWidgetHost.h>
#include <Components/PanelWidget.h>
#include <Components/TextBlock.h>
#include <Components/Widget.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Materials/MaterialInterface.h>

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

namespace {
template <typename... WidgetTypes>
void set_font_size_on_widgets(int32 const font_size, WidgetTypes* const... widgets) {
    auto const set_font_size{[font_size](auto* const widget) {
        if (IsValid(widget)) {
            widget->set_font_size(font_size);
            return;
        }

        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UShipHudWidget::set_common_widget_properties: Widget is invalid: %s"),
               *GetNameSafe(widget));
    }};
    (set_font_size(widgets), ...);
}
}

namespace ml::ship_hud {
inline constexpr float radar_display_dimension{640.0f};
inline constexpr float radar_viewport_margin{32.0f};

void configure_radar_canvas_slot(UWidget* const widget) {
    auto* const slot{Cast<UCanvasPanelSlot>(widget->Slot)};
    if (slot == nullptr) {
        return;
    }

    slot->SetAnchors(FAnchors{1.0f, 1.0f});
    slot->SetAlignment(FVector2D{1.0, 1.0});
    slot->SetPosition(FVector2D{-radar_viewport_margin, -radar_viewport_margin});
    slot->SetSize(FVector2D{radar_display_dimension, radar_display_dimension});
    slot->SetAutoSize(false);
}
} // namespace ml::ship_hud

auto UShipHudWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const hud_content{Super::RebuildWidget()};
    if (IsValid(radar_background)) {
        radar_background->SetBrushColor(FLinearColor::Transparent);
        ml::ship_hud::configure_radar_canvas_slot(radar_background);
    } else {
        UE_LOG(LogSandboxUI, Error, TEXT("Ship HUD has no radar background."));
    }
    auto const radar{SAssignNew(radar_widget_, SRadarWidget)};
    radar_widget_->set_frame_store(radar_frame_store_);
    radar_widget_->set_style(radar_style_);
    radar_widget_->SetVisibility(radar_frame_store_.IsValid() ? EVisibility::HitTestInvisible
                                                              : EVisibility::Collapsed);
    if (IsValid(radar_host)) {
        ml::ship_hud::configure_radar_canvas_slot(radar_host);
        radar_host->SetContent(radar);
    } else {
        UE_LOG(LogSandboxUI, Error, TEXT("Ship HUD has no radar NativeWidgetHost."));
    }
    return hud_content;
}

void UShipHudWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    radar_widget_.Reset();
}

void UShipHudWidget::NativeTick(FGeometry const& geometry, float const delta_time) {
    Super::NativeTick(geometry, delta_time);

    if (radar_widget_.IsValid() && radar_frame_store_.IsValid()) {
        radar_widget_->render();
    }
}

void UShipHudWidget::set_radar_frame_store(FRadarFrameStoreConstPtr frame_store) {
    radar_frame_store_ = MoveTemp(frame_store);
    if (radar_widget_.IsValid()) {
        radar_widget_->set_frame_store(radar_frame_store_);
        radar_widget_->SetVisibility(radar_frame_store_.IsValid() ? EVisibility::HitTestInvisible
                                                                  : EVisibility::Collapsed);
    }
}

void UShipHudWidget::set_radar_style(FRadarStyle const& style) {
    radar_style_ = style;
    apply_radar_colours();
    if (radar_widget_.IsValid()) {
        radar_widget_->set_style(radar_style_);
    }
}

void UShipHudWidget::NativeConstruct() {
    Super::NativeConstruct();
    set_common_widget_properties();

    RETURN_IF_NULLPTR(crosshair_material);
    RETURN_IF_NULLPTR(far_crosshair_widget);
    RETURN_IF_NULLPTR(near_crosshair_widget);
    RETURN_IF_NULLPTR(force_status_widget);

    near_crosshair_material_instance = UMaterialInstanceDynamic::Create(crosshair_material, this);
    far_crosshair_material_instance = UMaterialInstanceDynamic::Create(crosshair_material, this);

    RETURN_IF_NULLPTR(near_crosshair_material_instance);
    RETURN_IF_NULLPTR(far_crosshair_material_instance);

    near_crosshair_widget->SetBrushFromMaterial(near_crosshair_material_instance);
    far_crosshair_widget->SetBrushFromMaterial(far_crosshair_material_instance);
    update_crosshair_colours();
}

void UShipHudWidget::NativePreConstruct() {
    Super::NativePreConstruct();
    set_common_widget_properties();
}

void UShipHudWidget::set_common_widget_properties() {
    set_font_size_on_widgets(font_size,
                             health_widget,
                             points_widget,
                             stopwatch_widget,
                             fire_rate_widget,
                             target_speed_widget,
                             selected_imc_widget,
                             turning_widget,
                             moving_widget,
                             desired_velocity_scale_widget,
                             ship_velocity_widget,
                             target_velocity_widget,
                             control_mode_widget,
                             flight_mode_widget,
                             mission_status_panel);
}

void UShipHudWidget::apply_ui_style(ml::ioj::FGameUiStyle const& style) {
    Super::apply_ui_style(style);
    auto const& hud_style{style.hud()};
    reticle_normal_colour_ = hud_style.reticle_normal;
    reticle_warning_colour_ = hud_style.reticle_warning;
    reticle_danger_colour_ = hud_style.reticle_danger;

    if (speed_widget) {
        speed_widget->apply_hud_style(hud_style);
    }
    if (health_widget) {
        health_widget->apply_hud_style(hud_style);
    }
    if (energy_widget) {
        energy_widget->apply_hud_style(hud_style);
    }
    if (points_widget) {
        points_widget->apply_hud_style(hud_style);
    }

    auto apply_value_style{[](UValueWidget* const widget, FTextBlockStyle const& text_style) {
        if (widget) {
            widget->set_text_style(text_style);
        }
    }};
    apply_value_style(stopwatch_widget, hud_style.secondary_text);
    apply_value_style(fire_rate_widget, hud_style.secondary_text);
    apply_value_style(target_speed_widget, hud_style.secondary_text);
    apply_value_style(selected_imc_widget, hud_style.caption_text);
    apply_value_style(ship_velocity_widget, hud_style.secondary_text);
    apply_value_style(target_velocity_widget, hud_style.secondary_text);
    apply_value_style(control_mode_widget, hud_style.secondary_text);
    apply_value_style(flight_mode_widget, hud_style.secondary_text);

    stopwatch_widget->set_format_spec(TEXT("MISSION TIME // {0}:{1}:{2}"));
    fire_rate_widget->set_format_spec(TEXT("FIRE RATE // {0}"));
    target_speed_widget->set_format_spec(TEXT("TARGET SPEED // {0}"));
    selected_imc_widget->set_format_spec(TEXT("IMC // {0}"));
    ship_velocity_widget->set_format_spec(TEXT("VELOCITY // {0}"));
    target_velocity_widget->set_format_spec(TEXT("TARGET VELOCITY // {0} ({1})"));
    control_mode_widget->set_format_spec(TEXT("CONTROL // {0}"));
    flight_mode_widget->set_format_spec(TEXT("FLIGHT MODE // {0}"));

    for (auto* const widget : {turning_widget, moving_widget, desired_velocity_scale_widget}) {
        if (widget) {
            widget->apply_hud_style(hud_style);
        }
    }
#if WITH_EDITORONLY_DATA
    if (speed_graph) {
        speed_graph->apply_hud_style(hud_style);
    }
#endif

    if (WidgetTree) {
        WidgetTree->ForEachWidget([&hud_style](UWidget* const widget) {
            if (auto* const heading{Cast<UTextBlock>(widget)}) {
                ml::ioj::apply_text_style(*heading, hud_style.heading_text);
            }
        });
    }
    if (lock_on_widget) {
        lock_on_widget->SetColorAndOpacity(hud_style.reticle_danger);
    }

    apply_radar_colours();
    update_crosshair_colours();
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

#if WITH_EDITOR
void UShipHudWidget::update_sampled_speed(TConstArrayView<FVector2d> const samples,
                                          int32 const oldest_index) {
    RETURN_IF_NULLPTR(speed_graph);
    speed_graph->set_samples(samples, oldest_index);
}
#endif

void UShipHudWidget::set_crosshair_positions(FVector2d near, FVector2d far) {
    RETURN_IF_NULLPTR(far_crosshair_widget);
    RETURN_IF_NULLPTR(near_crosshair_widget);

    TRY_INIT_PTR(far_slot, Cast<UCanvasPanelSlot>(far_crosshair_widget->Slot));
    TRY_INIT_PTR(near_slot, Cast<UCanvasPanelSlot>(near_crosshair_widget->Slot));

    far_slot->SetPosition(far);
    near_slot->SetPosition(near);
}
void UShipHudWidget::set_crosshair_targeting(bool const targeting) {
    crosshair_targeting_ = targeting;
    update_crosshair_colours();
}

void UShipHudWidget::update_crosshair_colours() {
    RETURN_IF_NULLPTR(near_crosshair_material_instance);
    RETURN_IF_NULLPTR(far_crosshair_material_instance);

    FName const name{TEXT("colour")};

    UE_LOG(LogSandboxUI, Verbose, TEXT("Setting colour parameters."));

    near_crosshair_material_instance->SetVectorParameterValue(
        name, crosshair_targeting_ ? reticle_warning_colour_ : reticle_normal_colour_);
    far_crosshair_material_instance->SetVectorParameterValue(
        name, crosshair_targeting_ ? reticle_danger_colour_ : reticle_normal_colour_);
}

void UShipHudWidget::apply_radar_colours() {
    if (!has_ui_style()) {
        return;
    }

    radar_style_.objective_color = entity_overlay_defend_colour();
    radar_style_.player_color = entity_overlay_defend_colour();
    radar_style_.structure_color = entity_overlay_defend_colour();
    radar_style_.plane_color = FLinearColor::Transparent;
    if (radar_widget_.IsValid()) {
        radar_widget_->set_style(radar_style_);
    }
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
