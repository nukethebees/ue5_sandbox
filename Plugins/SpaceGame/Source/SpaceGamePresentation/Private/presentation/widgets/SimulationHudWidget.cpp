#include <SpaceGamePresentation/presentation/widgets/SimulationHudWidget.h>

#include <SandboxUI/EntityOverlay/SEntityOverlayWidget.h>
#include <SpaceGamePresentation/presentation/widgets/ForceStatusWidget.h>
#include <SpaceGamePresentation/presentation/widgets/MissionStatusWidget.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

#include <Engine/GameViewportClient.h>
#include <Engine/LocalPlayer.h>
#include <SceneView.h>
#include <Widgets/SOverlay.h>

auto USimulationHudWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const hud_content{Super::RebuildWidget()};
    auto const overlay{SAssignNew(entity_overlay_widget_, SEntityOverlayWidget)};
    entity_overlay_widget_->SetVisibility(entity_overlay_frame_store_.IsValid()
                                              ? EVisibility::HitTestInvisible
                                              : EVisibility::Collapsed);
    entity_overlay_widget_->set_frame_store(entity_overlay_frame_store_);
    entity_overlay_widget_->set_style(entity_overlay_style_);
    return SNew(SOverlay) + SOverlay::Slot()[overlay] + SOverlay::Slot()[hud_content];
}

void USimulationHudWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    entity_overlay_widget_.Reset();
}

void USimulationHudWidget::NativeTick(FGeometry const& geometry, float const delta_time) {
    Super::NativeTick(geometry, delta_time);
    if (!entity_overlay_widget_.IsValid() || !entity_overlay_frame_store_.IsValid()) {
        return;
    }

    FEntityOverlayView view;
    if (!try_get_entity_overlay_view(view)) {
        return;
    }
    entity_overlay_widget_->render(view);
}

auto USimulationHudWidget::try_get_entity_overlay_view(FEntityOverlayView& view) const -> bool {
    auto const* const local_player{GetOwningLocalPlayer()};
    auto* const viewport{IsValid(local_player) && IsValid(local_player->ViewportClient)
                             ? local_player->ViewportClient->Viewport
                             : nullptr};
    if (viewport == nullptr) {
        return false;
    }

    FSceneViewProjectionData projection_data;
    if (!local_player->GetProjectionData(viewport, projection_data) ||
        !projection_data.IsValidViewRectangle()) {
        return false;
    }

    view.camera_origin = FVector3f{projection_data.ViewOrigin};
    view.view_projection =
        FMatrix44f{projection_data.ViewRotationMatrix * projection_data.ProjectionMatrix};
    view.view_rect = projection_data.GetConstrainedViewRect();
    view.output_size = viewport->GetSizeXY();
    return view.is_valid();
}

void USimulationHudWidget::set_entity_overlay_frame_store(
    FEntityOverlayFrameStoreConstPtr frame_store) {
    entity_overlay_frame_store_ = MoveTemp(frame_store);
    if (entity_overlay_widget_.IsValid()) {
        entity_overlay_widget_->set_frame_store(entity_overlay_frame_store_);
        entity_overlay_widget_->SetVisibility(entity_overlay_frame_store_.IsValid()
                                                  ? EVisibility::HitTestInvisible
                                                  : EVisibility::Collapsed);
    }
}

void USimulationHudWidget::set_entity_overlay_style(FEntityOverlayStyle const& style) {
    entity_overlay_style_ = style;
    apply_entity_overlay_colours();
    if (entity_overlay_widget_.IsValid()) {
        entity_overlay_widget_->set_style(entity_overlay_style_);
    }
}

void USimulationHudWidget::apply_ui_style(ml::ioj::FGameUiStyle const& style) {
    auto const& hud_style{style.hud()};
    auto const& palette{style.palette()};
    has_ui_style_ = true;
    entity_overlay_background_colour_ = hud_style.control_background.TintColor.GetSpecifiedColor();
    entity_overlay_fill_colour_ = hud_style.health_nominal;
    entity_overlay_defend_colour_ = hud_style.objective_defend;
    entity_overlay_destroy_colour_ = hud_style.objective_destroy;
    entity_overlay_soft_target_neutral_colour_ = palette.text_secondary;
    entity_overlay_soft_target_in_range_colour_ = palette.honey;

    if (force_status_widget) {
        force_status_widget->apply_hud_style(hud_style);
    }
    if (mission_status_panel) {
        mission_status_panel->apply_hud_style(hud_style);
    }
    apply_entity_overlay_colours();
}

void USimulationHudWidget::set_entity_counts(FTestEntityRegistry::EntityCounts const& counts) {
    if (force_status_widget) {
        force_status_widget->set_entity_counts(counts);
    }
}

void USimulationHudWidget::set_entity_colours(UTestTeamVisualData::FColourArray const& colours) {
    if (force_status_widget) {
        force_status_widget->set_team_colours(colours);
    }
}

void USimulationHudWidget::set_mission_data(ml::hud_manager::FMissionDataCache const& data) {
    if (mission_status_panel) {
        mission_status_panel->set_mission_data(data);
    }
}

void USimulationHudWidget::apply_entity_overlay_colours() {
    if (!has_ui_style_) {
        return;
    }

    entity_overlay_style_.background_color = entity_overlay_background_colour_;
    entity_overlay_style_.fill_color = entity_overlay_fill_colour_;
    entity_overlay_style_.defend_objective_color = entity_overlay_defend_colour_;
    entity_overlay_style_.destroy_objective_color = entity_overlay_destroy_colour_;
    entity_overlay_style_.soft_target_neutral_color = entity_overlay_soft_target_neutral_colour_;
    entity_overlay_style_.soft_target_in_range_color = entity_overlay_soft_target_in_range_colour_;
    if (entity_overlay_widget_.IsValid()) {
        entity_overlay_widget_->set_style(entity_overlay_style_);
    }
}
