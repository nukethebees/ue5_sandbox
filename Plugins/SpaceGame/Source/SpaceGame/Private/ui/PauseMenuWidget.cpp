#include "SpaceGame/ui/PauseMenuWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/common/MenuButtonWidget.h"
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

namespace {
void apply_pause_menu_text_style(UTextBlock& text, FTextBlockStyle const& style) {
    text.SetFont(style.Font);
    text.SetColorAndOpacity(style.ColorAndOpacity);
    text.SetShadowOffset(style.ShadowOffset);
    text.SetShadowColorAndOpacity(style.ShadowColorAndOpacity);
    text.SetStrikeBrush(style.StrikeBrush);
    text.SetTextTransformPolicy(style.TransformPolicy);
    text.SetTextOverflowPolicy(style.OverflowPolicy);
}

template <typename Data>
auto make_graph_series(FText name,
                       Data const& data,
                       double const tick_period,
                       FLinearColor const color) -> FGraphSeries {
    FGraphSeries series;
    series.name = MoveTemp(name);
    series.style = {.color = color,
                    .thickness = 1.5f,
                    .antialias = true,
                    .interpolation = EGraphSeriesInterpolation::StepAfter};

    auto const sample_count{data.num()};
    series.x.Reserve(sample_count);
    series.y.Reserve(sample_count);
    for (int32 i{0}; i < sample_count; ++i) {
        series.x.Add(static_cast<float>(static_cast<double>(data.time_at(i)) * tick_period));
        series.y.Add(static_cast<float>(data.value_at(i)));
    }
    return series;
}
}

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

    apply_pause_menu_text_style(*paused_heading, style.text(EGameTextStyle::Heading1));
    apply_pause_menu_text_style(*page_heading, style.text(EGameTextStyle::Heading1));
    apply_pause_menu_text_style(*overview_placeholder, style.text(EGameTextStyle::BodySecondary));
    apply_pause_menu_text_style(*options_placeholder, style.text(EGameTextStyle::BodySecondary));
    apply_pause_menu_text_style(*stats_summary_heading, style.text(EGameTextStyle::Heading3));
    apply_pause_menu_text_style(*stats_graph_heading, style.text(EGameTextStyle::Heading3));
    apply_pause_menu_text_style(*stats_graph_description, style.text(EGameTextStyle::Caption));
    auto const& label_style{style.text(EGameTextStyle::BodySecondary)};
    apply_pause_menu_text_style(*stats_label_elapsed_time, label_style);
    apply_pause_menu_text_style(*stats_label_entities_spawned, label_style);
    apply_pause_menu_text_style(*stats_label_entities_active, label_style);
    apply_pause_menu_text_style(*stats_label_entities_destroyed, label_style);
    apply_pause_menu_text_style(*stats_label_kills, label_style);
    apply_pause_menu_text_style(*stats_label_lasers_fired, label_style);
    apply_pause_menu_text_style(*stats_label_lasers_active, label_style);

    auto const& value_style{style.text(EGameTextStyle::Body)};
    apply_pause_menu_text_style(*elapsed_time_value, value_style);
    apply_pause_menu_text_style(*entities_spawned_value, value_style);
    apply_pause_menu_text_style(*entities_active_value, value_style);
    apply_pause_menu_text_style(*entities_destroyed_value, value_style);
    apply_pause_menu_text_style(*kills_value, value_style);
    apply_pause_menu_text_style(*lasers_fired_value, value_style);
    apply_pause_menu_text_style(*lasers_active_value, value_style);

    stats_summary_panel->SetBrush(style.panel().background);
    stats_summary_panel->SetPadding(style.panel().padding);

    if (!stats_graph_.IsValid()) {
        return;
    }

    auto graph_style{FGraphPlotStyle{}};
    graph_style.desired_size = {640.0f, 260.0f};
    graph_style.label_font = style.text(EGameTextStyle::Caption).Font;
    graph_style.label_color =
        style.text(EGameTextStyle::Caption).ColorAndOpacity.GetSpecifiedColor();
    graph_style.axis_color =
        style.text(EGameTextStyle::BodySecondary).ColorAndOpacity.GetSpecifiedColor();
    graph_style.grid_color = graph_style.axis_color.CopyWithNewOpacity(0.25f);
    auto const panel_color{style.panel().background.TintColor.GetSpecifiedColor()};
    graph_style.background_color = panel_color;
    graph_style.plot_color = panel_color.CopyWithNewOpacity(panel_color.A * 0.35f);
    graph_style.empty_text =
        NSLOCTEXT("PauseMenu", "StatsGraphEmpty", "No level activity recorded");
    active_entity_series_color_ =
        style.text(EGameTextStyle::HudPrimary).ColorAndOpacity.GetSpecifiedColor();
    kills_series_color_ = style.text(EGameTextStyle::Warning).ColorAndOpacity.GetSpecifiedColor();
    (void)stats_graph_->set_style(MoveTemp(graph_style));
}

void UPauseMenuWidget::update_stats_view() {
    if (!IsValid(elapsed_time_value) || !IsValid(entities_spawned_value) ||
        !IsValid(entities_active_value) || !IsValid(entities_destroyed_value) ||
        !IsValid(kills_value) || !IsValid(lasers_fired_value) || !IsValid(lasers_active_value)) {
        return;
    }

    elapsed_time_value->SetText(format_elapsed_time(stats_snapshot_.elapsed_seconds));
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

    auto const tick_period{stats_snapshot_.tick_period};
    if (tick_period <= 0.0) {
        stats_graph_->clear_series();
        return;
    }

    TArray<FGraphSeries> series;
    series.Reserve(2);
    series.Add(make_graph_series(NSLOCTEXT("PauseMenu", "ActiveEntitiesSeries", "Active entities"),
                                 stats_snapshot_.active_entity_count_data,
                                 tick_period,
                                 active_entity_series_color_));
    series.Add(make_graph_series(NSLOCTEXT("PauseMenu", "KillsSeries", "Kills"),
                                 stats_snapshot_.cumulative_kill_count_data,
                                 tick_period,
                                 kills_series_color_));
    stats_graph_->set_series(MoveTemp(series));

    auto const x_max{FMath::Max(stats_snapshot_.elapsed_seconds, 1.0)};
    (void)stats_graph_->set_axis_settings(
        {.range_mode = EGraphRangeMode::Fixed, .fixed_range = {0.0, x_max}},
        {.range_mode = EGraphRangeMode::AutoIncludeZero});
}

auto UPauseMenuWidget::format_elapsed_time(double const elapsed_seconds) -> FText {
    auto const total_seconds{FMath::Max(FMath::FloorToInt64(elapsed_seconds), int64{0})};
    auto const seconds{total_seconds % 60};
    auto const total_minutes{total_seconds / 60};
    auto const minutes{total_minutes % 60};
    auto const hours{total_minutes / 60};
    if (hours > 0) {
        return FText::FromString(
            FString::Printf(TEXT("%lld:%02lld:%02lld"), hours, minutes, seconds));
    }
    return FText::FromString(FString::Printf(TEXT("%02lld:%02lld"), minutes, seconds));
}
}
