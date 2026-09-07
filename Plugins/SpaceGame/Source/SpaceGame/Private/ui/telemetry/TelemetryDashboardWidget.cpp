#include "SpaceGame/ui/telemetry/TelemetryDashboardWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/style/SpaceGameUiTheme.h"
#include "STelemetryDashboardView.h"

#include <Engine/GameInstance.h>

namespace ml::ioj {
UTelemetryDashboardWidget::UTelemetryDashboardWidget(FObjectInitializer const& object_initializer)
    : Super(object_initializer) {
    SetIsFocusable(true);
}

void UTelemetryDashboardWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    auto* const game_instance{GetGameInstance()};
    game_ = IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr;
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

void UTelemetryDashboardWidget::refresh() {
    catalog_.refresh();
    rebuild_state();
    publish();
}

bool UTelemetryDashboardWidget::select_run(FString const& run_id) {
    auto const selected{catalog_.select_run(run_id)};
    rebuild_state();
    publish();
    return selected;
}

void UTelemetryDashboardWidget::select_level_filter(FString const& level_label) {
    catalog_.set_level_filter(level_label);
    rebuild_state();
    publish();
}

void UTelemetryDashboardWidget::select_baseline(FString const& run_id) {
    (void)catalog_.select_baseline(run_id);
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

void UTelemetryDashboardWidget::rebuild_state() {
    state_ = FTelemetryDashboardViewState{};
    state_.runs = catalog_.get_runs();
    state_.level_filters = catalog_.get_level_filters();
    state_.baseline_runs = catalog_.get_baseline_candidates();
    state_.selected_baseline_run_id = catalog_.get_baseline_run_id();
    state_.selected_run_id = catalog_.get_selected_run_id();
    state_.selected_level_filter = catalog_.get_level_filter();
    state_.unreadable_files = catalog_.get_unreadable_file_count();
    state_.directory_exists = catalog_.directory_exists();
    state_.error = catalog_.get_selected_error();
    if (!external_error_.IsEmpty()) {
        state_.error =
            state_.error.IsEmpty() ? external_error_ : external_error_ + TEXT("\n") + state_.error;
    }
    auto const* const record{catalog_.get_selected_record()};
    if (!record) {
        return;
    }
    auto const level{FTelemetryRunSummary{.map_name = record->metadata.map_name,
                                          .level_id = record->metadata.level_id,
                                          .level_display_name = record->metadata.level_display_name}
                         .level_label()};
    state_.header = FText::FromString(FString::Printf(
        TEXT("%s\nLAUNCHED // %s    END // %s\nREQUESTED // "
             "%.3gx    MODE // %s    DURATION // %s\nBUILD // %s    "
             "PLATFORM // %s\nSOURCE SHA-256 // %s\nRUN // %s"),
        *level,
        *record->metadata.launched_utc,
        LexToSerializedString(record->completion.reason),
        record->metadata.initial_requested_time_scale,
        *record->metadata.presentation_mode,
        record->metadata.requested_duration_seconds.IsSet()
            ? *FString::Printf(TEXT("%.3fs"),
                               record->metadata.requested_duration_seconds.GetValue())
            : TEXT("unlimited"),
        *record->metadata.environment.build_configuration,
        *record->metadata.environment.platform,
        record->metadata.source_sha256.IsEmpty() ? TEXT("unavailable")
                                                 : *record->metadata.source_sha256,
        *record->metadata.run_id));
    state_.analysis = analyze_level_telemetry_run(*record);
    auto const* const baseline{catalog_.get_baseline_record()};
    if (baseline) {
        state_.baseline_analysis = analyze_level_telemetry_run(*baseline);
        TArray<FString> warnings;
        if (baseline->metadata.environment.build_version !=
            record->metadata.environment.build_version) {
            warnings.Add(TEXT("different builds"));
        }
        if (baseline->metadata.source_sha256 != record->metadata.source_sha256) {
            warnings.Add(TEXT("different S7 source"));
        }
        if (baseline->metadata.initial_requested_time_scale !=
            record->metadata.initial_requested_time_scale) {
            warnings.Add(TEXT("different requested speeds"));
        }
        if (baseline->metadata.presentation_mode != record->metadata.presentation_mode) {
            warnings.Add(TEXT("different presentation modes"));
        }
        if (!warnings.IsEmpty()) {
            state_.compatibility_warning = FText::FromString(TEXT("BASELINE COMPATIBILITY // ") +
                                                             FString::Join(warnings, TEXT(", ")));
        }
    }
    auto const* requested_ratio{
        state_.analysis.find_metric(ETelemetryDashboardMetric::RequestedTimeScaleRatio)};
    auto const* observed{state_.analysis.find_metric(ETelemetryDashboardMetric::ObservedTimeScale)};
    auto format_mean = [](FTelemetryMetricSeries const* metric) {
        return metric && metric->weighted_mean.IsSet()
                 ? FString::Printf(TEXT("%.3g"), metric->weighted_mean.GetValue())
                 : FString{TEXT("—")};
    };
    auto last_int = [](FLevelTelemetryTickSeries::Int32Data const& series) {
        return series.is_empty() ? 0 : series.last_value();
    };
    auto last_uint = [](FLevelTelemetryTickSeries::Uint64Data const& series) {
        return series.is_empty() ? uint64{0} : series.last_value();
    };
    state_.summary = FText::FromString(FString::Printf(
        TEXT("COMPLETED TICKS  //  %llu\nSIMULATED / REAL  //  %.3fs / %.3fs\nMEAN OBSERVED  //  "
             "%sx\nMEAN REQUESTED-SCALE RATIO  //  %s%%\nFINAL WORKLOAD  //  ENTITIES %d  LASERS "
             "%d  SLOTS "
             "%d  "
             "CELLS %d\nFINAL COUNTERS  //  SPAWN %d  DESTROY %d  KILLS %d  FIRED %d\nQUERIES  //  "
             "GRID %llu  RANGE %llu  LINE %llu  SWEEP %llu"),
        record->completion.completed_ticks,
        record->completion.simulated_elapsed_seconds,
        record->completion.wall_elapsed_seconds,
        *format_mean(observed),
        *format_mean(requested_ratio),
        last_int(record->tick_series.active_entities),
        last_int(record->tick_series.active_lasers),
        last_int(record->tick_series.registry_slot_count),
        last_int(record->tick_series.occupied_spatial_cell_count),
        last_int(record->tick_series.spawned_entities),
        last_int(record->tick_series.destroyed_entities),
        last_int(record->tick_series.kills),
        last_int(record->tick_series.lasers_fired),
        last_uint(record->tick_series.grid_rebuild_count),
        last_uint(record->tick_series.range_query_count),
        last_uint(record->tick_series.line_trace_count),
        last_uint(record->tick_series.sweep_trace_count)));
    auto summary{state_.summary.ToString()};
    if (record->loaded_schema_version < 2 || record->battle_samples.IsEmpty()) {
        summary += TEXT("\nBATTLE / PERFORMANCE METRICS  //  UNAVAILABLE (LEGACY V1 RUN)");
    } else {
        auto sum_counts = [](auto const& counts) {
            using Result = std::remove_cvref_t<decltype(counts[0][0])>;
            Result total{};
            for (auto const& row : counts) {
                for (auto const value : row) {
                    total += value;
                }
            }
            return total;
        };
        auto const& battle{record->battle_samples.Last()};
        auto const& combat{battle.combat};
        auto const winner{
            record->completion.reason == ELevelTelemetryRunEndReason::BattleResolved
                ? (record->completion.winning_team.IsSet()
                       ? FString{LexToSerializedString(record->completion.winning_team.GetValue())}
                       : FString{TEXT("draw")})
                : FString{TEXT("—")}};
        summary += FString::Printf(
            TEXT("\nBATTLE RESULT  //  %s    SAMPLES // %d\nCOMBAT  //  SHOTS %llu  HITS %llu  "
                 "DAMAGE %.0f  KILLS %llu  LOSSES %llu\nPERFORMANCE WINDOWS  //  %d    "
                 "DETAILED TIMING // %s"),
            *winner,
            record->battle_samples.Num(),
            sum_counts(combat.shots),
            sum_counts(combat.hits),
            sum_counts(combat.damage_dealt),
            sum_counts(combat.kills),
            sum_counts(combat.losses),
            record->performance_windows.Num(),
            record->metadata.detailed_timing ? TEXT("ON") : TEXT("OFF"));
        if (!record->performance_windows.IsEmpty()) {
            auto const& window{record->performance_windows.Last()};
            summary += FString::Printf(
                TEXT("\nLATEST WINDOW  //  FRAME MEAN/P95/MAX %.3f/%.3f/%.3f ms    "
                     "SIM TICK MEAN/MAX %.3f/%.3f ms\nPHASE CPU SHARE  //  SETUP %.1f%%  "
                     "DECISION %.1f%%  SIMULATION %.1f%%  RESOLUTION %.1f%%  END %.1f%%"),
                window.frame.mean_ms,
                window.frame.p95_ms,
                window.frame.max_ms,
                window.simulation_tick.mean_ms,
                window.simulation_tick.max_ms,
                window.phase_cpu_share[0] * 100.0,
                window.phase_cpu_share[1] * 100.0,
                window.phase_cpu_share[2] * 100.0,
                window.phase_cpu_share[3] * 100.0,
                window.phase_cpu_share[4] * 100.0);
        }
    }
    if (baseline) {
        auto const* baseline_observed{
            state_.baseline_analysis.find_metric(ETelemetryDashboardMetric::ObservedTimeScale)};
        if (observed && observed->weighted_mean.IsSet() && baseline_observed &&
            baseline_observed->weighted_mean.IsSet()) {
            auto const current{observed->weighted_mean.GetValue()};
            auto const base{baseline_observed->weighted_mean.GetValue()};
            summary +=
                FString::Printf(TEXT("\nBASELINE OBSERVED SPEED  //  CURRENT %.3gx  BASE %.3gx  "
                                     "DELTA %+.3gx"),
                                current,
                                base,
                                current - base);
        }
    }
    state_.summary = FText::FromString(MoveTemp(summary));
}

void UTelemetryDashboardWidget::publish() {
    if (view_.IsValid()) {
        view_->replace_state(state_);
    }
}
} // namespace ml::ioj
