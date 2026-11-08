#include "SpaceGame/ui/telemetry/TelemetryDashboardWidget.h"

#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include "STelemetryDashboardView.h"
#include <ioj/sim/telemetry/run_report.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/GameInstance.h>

namespace ioj {
UTelemetryDashboardWidget::UTelemetryDashboardWidget(FObjectInitializer const& object_initializer)
    : Super(object_initializer) {
    SetIsFocusable(true);
}

/* **************************************** */
// Widget lifecycle
/* **************************************** */
void UTelemetryDashboardWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    auto* const game_instance{GetGameInstance()};
    game_ = UGameSubsystem::get(game_instance);
    if (!IsValid(game_)) {
        auto const* const theme{GetDefault<USpaceGameUiTheme>()};
        if (IsValid(theme)) {
            fallback_style_ = theme->compile();
        }
        UE_LOG(LogSandboxUI, Warning, TEXT("Telemetry dashboard is using the default UI theme."));
    }
}

auto UTelemetryDashboardWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const* const style{IsValid(game_) ? &game_->get_ui_style() : &fallback_style_};
    auto result{SAssignNew(view_, STelemetryDashboardView)
                    .Style(style)
                    .OnRefresh(FSimpleDelegate::CreateUObject(this, &ThisClass::refresh))
                    .OnRunSelected(FOnTelemetryRunSelected::CreateUObject(
                        this, &ThisClass::handle_run_selected))
                    .OnLevelFilterSelected(FOnTelemetryLevelFilterSelected::CreateUObject(
                        this, &ThisClass::handle_level_filter_selected))
                    .OnBaselineSelected(FOnTelemetryBaselineSelected::CreateUObject(
                        this, &ThisClass::handle_baseline_selected))};
    publish();
    return result;
}

void UTelemetryDashboardWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

auto UTelemetryDashboardWidget::NativeOnFocusReceived(FGeometry const&, FFocusEvent const&)
    -> FReply {
    focus_primary_action();
    return FReply::Handled();
}

/* **************************************** */
// Actions and selection callbacks
/* **************************************** */
void UTelemetryDashboardWidget::refresh() {
    catalog_.refresh();
    rebuild_state();
    publish();
}

bool UTelemetryDashboardWidget::select_run(FString const& run_id) {
    auto const selected{catalog_.select_run(to_string(run_id))};
    rebuild_state();
    publish();
    return selected;
}

void UTelemetryDashboardWidget::select_level_filter(FString const& level_label) {
    catalog_.set_level_filter(to_string(level_label));
    rebuild_state();
    publish();
}

void UTelemetryDashboardWidget::select_baseline(FString const& run_id) {
    (void)catalog_.select_baseline(to_string(run_id));
    rebuild_state();
    publish();
}

void UTelemetryDashboardWidget::set_external_error(FString error) {
    external_error_ = MoveTemp(error);
    rebuild_state();
    publish();
}

void UTelemetryDashboardWidget::handle_run_selected(FString run_id) {
    (void)select_run(run_id);
}

void UTelemetryDashboardWidget::handle_level_filter_selected(FString level_label) {
    select_level_filter(level_label);
}

void UTelemetryDashboardWidget::handle_baseline_selected(FString run_id) {
    select_baseline(run_id);
}

void UTelemetryDashboardWidget::focus_primary_action() {
    if (view_.IsValid()) {
        view_->focus_primary_action();
    }
}

/* **************************************** */
// State publication
/* **************************************** */
void UTelemetryDashboardWidget::rebuild_state() {
    state_ = FTelemetryDashboardViewState{};
    auto const append_summaries{[](auto const& summaries, auto& output) {
        for (auto const& summary : summaries) {
            output.Add(FTelemetryRunSummary{to_fstring(summary.run_id),
                                            to_fstring(summary.level_label()),
                                            to_fstring(summary.launched_utc),
                                            summary.completion_reason});
        }
    }};
    append_summaries(catalog_.runs(), state_.runs);
    for (auto const& label : catalog_.level_filters()) {
        state_.level_filters.Add(to_fstring(label));
    }
    append_summaries(catalog_.baseline_candidates(), state_.baseline_runs);
    state_.selected_baseline_run_id = to_fstring(catalog_.baseline_run_id());
    state_.selected_run_id = to_fstring(catalog_.selected_run_id());
    state_.selected_level_filter = to_fstring(catalog_.level_filter());
    state_.unreadable_files = static_cast<int32>(catalog_.unreadable_file_count());
    state_.error = to_fstring(catalog_.selected_error());
    if (!external_error_.IsEmpty()) {
        state_.error =
            state_.error.IsEmpty() ? external_error_ : external_error_ + TEXT("\n") + state_.error;
    }
    auto const* const record{catalog_.selected_record()};
    if (!record) {
        return;
    }
    auto const level{to_fstring(
        sim::telemetry::RunSummary{.map_name = record->run.metadata.map_name,
                                   .level_id = record->run.metadata.level_id,
                                   .level_display_name = record->run.metadata.level_display_name}
            .level_label())};
    auto const completion_reason{
        to_fstring(sim::to_serialized_string(record->run.completion.reason))};
    state_.header = FText::FromString(FString::Printf(
        TEXT("%s\nLAUNCHED // %s    END // %s\nMODE // %s    DURATION // %s\nRUN // %s"),
        *level,
        UTF8_TO_TCHAR(record->run.metadata.launched_utc.c_str()),
        *completion_reason,
        record->presentation_enabled ? TEXT("visual") : TEXT("simulation_only"),
        record->run.metadata.requested_duration_seconds.has_value()
            ? *FString::Printf(TEXT("%.3fs"),
                               record->run.metadata.requested_duration_seconds->seconds())
            : TEXT("unlimited"),
        UTF8_TO_TCHAR(record->run.metadata.run_id.c_str())));
    state_.analysis = analyze_level_telemetry_run(*record);
    auto const* const baseline{catalog_.baseline_record()};
    if (baseline) {
        state_.baseline_analysis = analyze_level_telemetry_run(*baseline);
        TArray<FString> warnings;
        auto const context{sim::telemetry::compare_run_context(*baseline, *record)};
        if (context.different_level_source) {
            warnings.Add(TEXT("different level source"));
        }
        if (context.different_settings_source) {
            warnings.Add(TEXT("different settings source"));
        }
        if (context.different_presentation_mode) {
            warnings.Add(TEXT("different presentation modes"));
        }
        if (!warnings.IsEmpty()) {
            state_.compatibility_warning = FText::FromString(TEXT("BASELINE COMPATIBILITY // ") +
                                                             FString::Join(warnings, TEXT(", ")));
        }
    }
    auto const statistics{sim::telemetry::summarize_run(*record)};
    auto summary{FString::Printf(
        TEXT(
            "COMPLETED TICKS  //  %llu\nSIMULATED TIME  //  %.3fs\nFINAL FORCES  //  "
            "ENTITIES %d  LASERS %d\nFINAL COUNTERS  //  SPAWN %d  DESTROY %d  KILLS %d  FIRED %d"),
        record->run.completion.completed_ticks.value(),
        statistics.simulated_seconds,
        statistics.active_entities,
        statistics.active_lasers,
        statistics.spawned_entities,
        statistics.destroyed_entities,
        statistics.kills,
        statistics.lasers_fired)};
    if (!statistics.level_complete) {
        summary += TEXT("\nMISSION  //  INCOMPLETE");
    }
    summary += FString::Printf(TEXT("\nBATTLE SAMPLES // %d\nCOMBAT  //  SHOTS %llu  HITS %llu  "
                                    "DAMAGE %.0f  KILLS %llu  LOSSES %llu"),
                               static_cast<int32>(record->run.battle_samples.size()),
                               statistics.shots,
                               statistics.hits,
                               statistics.damage_dealt,
                               statistics.combat_kills,
                               statistics.losses);
    state_.summary = FText::FromString(MoveTemp(summary));
}

void UTelemetryDashboardWidget::publish() {
    if (view_.IsValid()) {
        view_->replace_state(state_);
    }
}
} // namespace ioj
