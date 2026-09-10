#include "SpaceGame/ui/PauseMenuWidget.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include "LevelTelemetryPresentation.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/presentation/widgets/TeamEntityTableWidget.h"
#include "SpaceGamePresentation/presentation/widgets/TopKillersWidget.h"
#include "SpaceGamePresentation/ui/common/MenuButtonWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

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
/* **************************************** */
// Widget lifecycle
/* **************************************** */
void UPauseMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(resume_button) || !IsValid(overview_button) || !IsValid(forces_button) ||
        !IsValid(combat_button) || !IsValid(telemetry_button) || !IsValid(options_button) ||
        !IsValid(return_to_level_select_button) || !IsValid(quit_button) ||
        !IsValid(page_heading) || !IsValid(paused_heading) || !IsValid(page_switcher) ||
        !IsValid(options_placeholder) || !IsValid(overview_summary_panel) ||
        !IsValid(overview_summary_heading) || !IsValid(overview_label_elapsed_time) ||
        !IsValid(overview_label_entities_spawned) || !IsValid(overview_label_entities_active) ||
        !IsValid(overview_label_entities_destroyed) || !IsValid(overview_label_kills) ||
        !IsValid(elapsed_time_value) || !IsValid(entities_spawned_value) ||
        !IsValid(entities_active_value) || !IsValid(entities_destroyed_value) ||
        !IsValid(kills_value) || !IsValid(forces_counts_heading) || !IsValid(forces_table) ||
        !IsValid(combat_top_killers_heading) || !IsValid(combat_top_killers_table) ||
        !IsValid(combat_team_kills_heading) || !IsValid(combat_team_kills_table) ||
        !IsValid(telemetry_summary_panel) || !IsValid(telemetry_summary_heading) ||
        !IsValid(telemetry_label_lasers_fired) || !IsValid(telemetry_label_lasers_active) ||
        !IsValid(lasers_fired_value) || !IsValid(lasers_active_value) ||
        !IsValid(telemetry_graph_heading) || !IsValid(telemetry_graph_description) ||
        !IsValid(telemetry_graph_host)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    resume_button->OnClicked().AddUObject(this, &ThisClass::handle_resume);
    overview_button->OnClicked().AddUObject(this, &ThisClass::handle_overview);
    forces_button->OnClicked().AddUObject(this, &ThisClass::handle_forces);
    combat_button->OnClicked().AddUObject(this, &ThisClass::handle_combat);
    telemetry_button->OnClicked().AddUObject(this, &ThisClass::handle_telemetry);
    options_button->OnClicked().AddUObject(this, &ThisClass::handle_options);
    return_to_level_select_button->OnClicked().AddUObject(
        this, &ThisClass::handle_return_to_level_select);
    quit_button->OnClicked().AddUObject(this, &ThisClass::handle_quit);

    for (auto* const button :
         {overview_button, forces_button, combat_button, telemetry_button, options_button}) {
        button->SetIsSelectable(true);
        button->SetIsToggleable(true);
    }

    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (!IsValid(telemetry_graph_host)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::NativeConstruct: Telemetry graph host is invalid."));
        return;
    }

    if (!telemetry_graph_.IsValid()) {
        SAssignNew(telemetry_graph_, SGraphPlot);
    }
    telemetry_graph_host->SetContent(telemetry_graph_.ToSharedRef());
    apply_ui_style();
    update_views();
}

void UPauseMenuWidget::prepare_for_open(UInputAction& toggle_action, FPauseMenuData data) {
    toggle_action_ = &toggle_action;
    data_ = MoveTemp(data);
    terminal_action_requested_ = false;
    update_views();
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
    telemetry_graph_.Reset();
}

/* **************************************** */
// Navigation callbacks
/* **************************************** */
void UPauseMenuWidget::handle_resume() {
    if (!terminal_action_requested_) {
        DeactivateWidget();
    }
}

void UPauseMenuWidget::handle_overview() {
    set_active_tab(EPauseMenuTab::Overview);
}

void UPauseMenuWidget::handle_forces() {
    set_active_tab(EPauseMenuTab::Forces);
}

void UPauseMenuWidget::handle_combat() {
    set_active_tab(EPauseMenuTab::Combat);
}

void UPauseMenuWidget::handle_telemetry() {
    set_active_tab(EPauseMenuTab::Telemetry);
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
    if (!terminal_action_requested_) {
        DeactivateWidget();
    }
}

/* **************************************** */
// View state and presentation
/* **************************************** */
void UPauseMenuWidget::set_active_tab(EPauseMenuTab const tab) {
    if (!IsValid(page_heading) || !IsValid(page_switcher) || !IsValid(overview_button) ||
        !IsValid(forces_button) || !IsValid(combat_button) || !IsValid(telemetry_button) ||
        !IsValid(options_button)) {
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
        case EPauseMenuTab::Forces: {
            heading = NSLOCTEXT("PauseMenu", "ForcesHeading", "Forces");
            break;
        }
        case EPauseMenuTab::Combat: {
            heading = NSLOCTEXT("PauseMenu", "CombatHeading", "Combat");
            break;
        }
        case EPauseMenuTab::Telemetry: {
            heading = NSLOCTEXT("PauseMenu", "TelemetryHeading", "Telemetry");
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
    forces_button->SetIsSelected(tab == EPauseMenuTab::Forces);
    combat_button->SetIsSelected(tab == EPauseMenuTab::Combat);
    telemetry_button->SetIsSelected(tab == EPauseMenuTab::Telemetry);
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
    apply_text_style(*options_placeholder, style.text(EGameTextStyle::BodySecondary));
    for (auto* const heading : {overview_summary_heading,
                                forces_counts_heading,
                                combat_top_killers_heading,
                                combat_team_kills_heading,
                                telemetry_summary_heading,
                                telemetry_graph_heading}) {
        apply_text_style(*heading, style.text(EGameTextStyle::Heading3));
    }
    apply_text_style(*telemetry_graph_description, style.text(EGameTextStyle::Caption));

    auto const& label_style{style.text(EGameTextStyle::BodySecondary)};
    for (auto* const label : {overview_label_elapsed_time,
                              overview_label_entities_spawned,
                              overview_label_entities_active,
                              overview_label_entities_destroyed,
                              overview_label_kills,
                              telemetry_label_lasers_fired,
                              telemetry_label_lasers_active}) {
        apply_text_style(*label, label_style);
    }

    auto const& value_style{style.text(EGameTextStyle::Body)};
    for (auto* const value : {elapsed_time_value,
                              entities_spawned_value,
                              entities_active_value,
                              entities_destroyed_value,
                              kills_value,
                              lasers_fired_value,
                              lasers_active_value}) {
        apply_text_style(*value, value_style);
    }

    for (auto* const panel : {overview_summary_panel, telemetry_summary_panel}) {
        panel->SetBrush(style.panel().background);
        panel->SetPadding(style.panel().padding);
    }

    forces_table->apply_hud_style(style.hud());
    combat_top_killers_table->apply_hud_style(style.hud());
    combat_team_kills_table->apply_hud_style(style.hud());

    if (!telemetry_graph_.IsValid()) {
        return;
    }

    active_entity_series_color_ = style.palette().honey;
    kills_series_color_ = style.palette().danger;
    level_telemetry_presentation::apply_activity_graph_style(
        *telemetry_graph_,
        style,
        NSLOCTEXT("PauseMenu", "TelemetryGraphEmpty", "No level activity recorded"),
        {640.0f, 260.0f});
}

void UPauseMenuWidget::update_views() {
    if (!IsValid(elapsed_time_value) || !IsValid(entities_spawned_value) ||
        !IsValid(entities_active_value) || !IsValid(entities_destroyed_value) ||
        !IsValid(kills_value) || !IsValid(lasers_fired_value) || !IsValid(lasers_active_value) ||
        !IsValid(forces_table) || !IsValid(combat_top_killers_table) ||
        !IsValid(combat_team_kills_table)) {
        return;
    }

    auto const& telemetry{data_.telemetry};
    elapsed_time_value->SetText(
        level_telemetry_presentation::format_elapsed_time(telemetry.elapsed_seconds));
    entities_spawned_value->SetText(FText::AsNumber(telemetry.spawned_entities));
    entities_active_value->SetText(FText::AsNumber(telemetry.active_entities));
    entities_destroyed_value->SetText(FText::AsNumber(telemetry.destroyed_entities));
    kills_value->SetText(FText::AsNumber(telemetry.kills));
    lasers_fired_value->SetText(FText::AsNumber(telemetry.lasers_fired));
    lasers_active_value->SetText(FText::AsNumber(telemetry.active_lasers));

    forces_table->set_show_team_totals(true);
    forces_table->set_team_colours(data_.team_colours);
    forces_table->set_entity_counts(data_.alive_per_team_and_type);
    combat_top_killers_table->set_team_colours(data_.team_colours);
    combat_top_killers_table->set_top_killers(data_.top_killers);
    combat_team_kills_table->set_show_team_totals(true);
    combat_team_kills_table->set_team_colours(data_.team_colours);
    combat_team_kills_table->set_team_kill_matrix(data_.team_kill_matrix);
    update_telemetry_graph();
}

void UPauseMenuWidget::update_telemetry_graph() {
    if (telemetry_graph_.IsValid()) {
        level_telemetry_presentation::update_activity_graph(
            *telemetry_graph_, data_.telemetry, active_entity_series_color_, kills_series_color_);
    }
}
}
