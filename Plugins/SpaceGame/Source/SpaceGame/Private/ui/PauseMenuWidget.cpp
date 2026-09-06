#include "SpaceGame/ui/PauseMenuWidget.h"

#include "LevelTelemetryPresentation.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/common/MenuButtonWidget.h"
#include "SpaceGame/ui/style/GameUiStyle.h"
#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include "SandboxUI/widgets/SGraphPlot.h"

#include <Components/Border.h>
#include <Components/NativeWidgetHost.h>
#include <Components/TextBlock.h>
#include <Components/WidgetSwitcher.h>
#include <Engine/GameInstance.h>
#include <Input/CommonUIInputTypes.h>
#include <Input/UIActionBinding.h>
#include <InputAction.h>
#include <Widgets/DeclarativeSyntaxSupport.h>

namespace ml::ioj {
void UPauseMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(resume_button) || !IsValid(overview_button) || !IsValid(stats_button) ||
        !IsValid(options_button) || !IsValid(return_to_level_select_button) ||
        !IsValid(quit_button) || !IsValid(page_heading) || !IsValid(paused_heading) ||
        !IsValid(page_switcher) || !IsValid(overview_placeholder) ||
        !IsValid(options_placeholder) || !IsValid(stats_summary_panel) ||
        !IsValid(stats_summary_heading) || !IsValid(stats_label_elapsed_time) ||
        !IsValid(stats_label_entities_spawned) || !IsValid(stats_label_entities_active) ||
        !IsValid(stats_label_entities_destroyed) || !IsValid(stats_label_kills) ||
        !IsValid(stats_label_lasers_fired) || !IsValid(stats_label_lasers_active) ||
        !IsValid(elapsed_time_value) || !IsValid(entities_spawned_value) ||
        !IsValid(entities_active_value) || !IsValid(entities_destroyed_value) ||
        !IsValid(kills_value) || !IsValid(lasers_fired_value) || !IsValid(lasers_active_value) ||
        !IsValid(stats_graph_heading) || !IsValid(stats_graph_description) ||
        !IsValid(stats_graph_host)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    resume_button->OnClicked().AddUObject(this, &ThisClass::handle_resume);
    overview_button->OnClicked().AddUObject(this, &ThisClass::handle_overview);
    stats_button->OnClicked().AddUObject(this, &ThisClass::handle_stats);
    options_button->OnClicked().AddUObject(this, &ThisClass::handle_options);
    return_to_level_select_button->OnClicked().AddUObject(
        this, &ThisClass::handle_return_to_level_select);
    quit_button->OnClicked().AddUObject(this, &ThisClass::handle_quit);
    overview_button->SetIsSelectable(true);
    overview_button->SetIsToggleable(true);
    stats_button->SetIsSelectable(true);
    stats_button->SetIsToggleable(true);
    options_button->SetIsSelectable(true);
    options_button->SetIsToggleable(true);

    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (!IsValid(stats_graph_host)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::NativeConstruct: Stats graph host is invalid."));
        return;
    }

    if (!stats_graph_.IsValid()) {
        SAssignNew(stats_graph_, SGraphPlot);
    }
    stats_graph_host->SetContent(stats_graph_.ToSharedRef());
    apply_ui_style();
    update_stats_view();
}

void UPauseMenuWidget::prepare_for_open(UInputAction& toggle_action,
                                        FLevelTelemetrySnapshot snapshot) {
    toggle_action_ = &toggle_action;
    stats_snapshot_ = MoveTemp(snapshot);
    terminal_action_requested_ = false;
    update_stats_view();
    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::NativeOnActivated() {
    set_active_tab(EPauseMenuTab::Overview);
    if (auto* const toggle_action{toggle_action_.Get()}; IsValid(toggle_action)) {
        FBindUIActionArgs const args{
            toggle_action,
            false,
            FSimpleDelegate::CreateUObject(this, &ThisClass::handle_toggle_action)};
        toggle_action_binding_ = RegisterUIActionBinding(args);
    }
    Super::NativeOnActivated();
}

void UPauseMenuWidget::NativeOnDeactivated() {
    if (toggle_action_binding_.IsValid()) {
        toggle_action_binding_.Unregister();
        toggle_action_binding_ = {};
    }
    Super::NativeOnDeactivated();
}

auto UPauseMenuWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return resume_button;
}

void UPauseMenuWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    stats_graph_.Reset();
}

void UPauseMenuWidget::handle_resume() {
    if (terminal_action_requested_) {
        return;
    }
    DeactivateWidget();
}

void UPauseMenuWidget::handle_overview() {
    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::handle_stats() {
    set_active_tab(EPauseMenuTab::Stats);
}

void UPauseMenuWidget::handle_options() {
    set_active_tab(EPauseMenuTab::Options);
}

void UPauseMenuWidget::handle_return_to_level_select() {
    if (terminal_action_requested_) {
        return;
    }
    terminal_action_requested_ = true;
    return_to_level_select_requested.Broadcast();
}

void UPauseMenuWidget::handle_quit() {
    if (terminal_action_requested_) {
        return;
    }
    terminal_action_requested_ = true;
    quit_requested.Broadcast();
}

void UPauseMenuWidget::handle_toggle_action() {
    if (terminal_action_requested_) {
        return;
    }
    DeactivateWidget();
}

void UPauseMenuWidget::set_active_tab(EPauseMenuTab const tab) {
    if (!IsValid(page_heading) || !IsValid(page_switcher) || !IsValid(overview_button) ||
        !IsValid(stats_button) || !IsValid(options_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::set_active_tab: One or more bound widgets are invalid."));
        return;
    }

    FText heading;
    switch (tab) {
        case EPauseMenuTab::Overview: {
            heading = NSLOCTEXT("PauseMenu", "OverviewHeading", "Overview");
            break;
        }
        case EPauseMenuTab::Stats: {
            heading = NSLOCTEXT("PauseMenu", "StatsHeading", "Stats");
            break;
        }
        case EPauseMenuTab::Options: {
            heading = NSLOCTEXT("PauseMenu", "OptionsHeading", "Options");
            break;
        }
        default: {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UPauseMenuWidget::set_active_tab: Unhandled tab value %d."),
                   static_cast<int32>(tab));
            return;
        }
    }

    active_tab = tab;
    page_heading->SetText(heading);
    page_switcher->SetActiveWidgetIndex(static_cast<int32>(tab));
    overview_button->SetIsSelected(tab == EPauseMenuTab::Overview);
    stats_button->SetIsSelected(tab == EPauseMenuTab::Stats);
    options_button->SetIsSelected(tab == EPauseMenuTab::Options);
}

void UPauseMenuWidget::apply_ui_style() {
    FGameUiStyle style;
    auto const* const game_instance{GetGameInstance()};
    auto const* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr};
    if (IsValid(subsystem)) {
        style = subsystem->get_ui_style();
    } else {
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        style = default_theme->compile();
    }

    apply_text_style(*paused_heading, style.text(EGameTextStyle::Heading1));
    apply_text_style(*page_heading, style.text(EGameTextStyle::Heading1));
    apply_text_style(*overview_placeholder, style.text(EGameTextStyle::BodySecondary));
    apply_text_style(*options_placeholder, style.text(EGameTextStyle::BodySecondary));
    apply_text_style(*stats_summary_heading, style.text(EGameTextStyle::Heading3));
    apply_text_style(*stats_graph_heading, style.text(EGameTextStyle::Heading3));
    apply_text_style(*stats_graph_description, style.text(EGameTextStyle::Caption));
    auto const& label_style{style.text(EGameTextStyle::BodySecondary)};
    apply_text_style(*stats_label_elapsed_time, label_style);
    apply_text_style(*stats_label_entities_spawned, label_style);
    apply_text_style(*stats_label_entities_active, label_style);
    apply_text_style(*stats_label_entities_destroyed, label_style);
    apply_text_style(*stats_label_kills, label_style);
    apply_text_style(*stats_label_lasers_fired, label_style);
    apply_text_style(*stats_label_lasers_active, label_style);

    auto const& value_style{style.text(EGameTextStyle::Body)};
    apply_text_style(*elapsed_time_value, value_style);
    apply_text_style(*entities_spawned_value, value_style);
    apply_text_style(*entities_active_value, value_style);
    apply_text_style(*entities_destroyed_value, value_style);
    apply_text_style(*kills_value, value_style);
    apply_text_style(*lasers_fired_value, value_style);
    apply_text_style(*lasers_active_value, value_style);

    stats_summary_panel->SetBrush(style.panel().background);
    stats_summary_panel->SetPadding(style.panel().padding);

    if (!stats_graph_.IsValid()) {
        return;
    }

    active_entity_series_color_ = style.palette().honey;
    kills_series_color_ = style.palette().danger;
    level_telemetry_presentation::apply_activity_graph_style(
        *stats_graph_,
        style,
        NSLOCTEXT("PauseMenu", "StatsGraphEmpty", "No level activity recorded"),
        {640.0f, 260.0f});
}

void UPauseMenuWidget::update_stats_view() {
    if (!IsValid(elapsed_time_value) || !IsValid(entities_spawned_value) ||
        !IsValid(entities_active_value) || !IsValid(entities_destroyed_value) ||
        !IsValid(kills_value) || !IsValid(lasers_fired_value) || !IsValid(lasers_active_value)) {
        return;
    }

    elapsed_time_value->SetText(
        level_telemetry_presentation::format_elapsed_time(stats_snapshot_.elapsed_seconds));
    entities_spawned_value->SetText(FText::AsNumber(stats_snapshot_.spawned_entities));
    entities_active_value->SetText(FText::AsNumber(stats_snapshot_.active_entities));
    entities_destroyed_value->SetText(FText::AsNumber(stats_snapshot_.destroyed_entities));
    kills_value->SetText(FText::AsNumber(stats_snapshot_.kills));
    lasers_fired_value->SetText(FText::AsNumber(stats_snapshot_.lasers_fired));
    lasers_active_value->SetText(FText::AsNumber(stats_snapshot_.active_lasers));
    update_stats_graph();
}

void UPauseMenuWidget::update_stats_graph() {
    if (!stats_graph_.IsValid()) {
        return;
    }

    level_telemetry_presentation::update_activity_graph(
        *stats_graph_, stats_snapshot_, active_entity_series_color_, kills_series_color_);
}
}
