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
                        this, &ThisClass::handle_level_filter_selected))};
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

void UTelemetryDashboardWidget::handle_run_selected(FString run_id) {
    (void)select_run(run_id);
}

void UTelemetryDashboardWidget::handle_level_filter_selected(FString level_label) {
    select_level_filter(level_label);
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
    state_.selected_run_id = catalog_.get_selected_run_id();
    state_.selected_level_filter = catalog_.get_level_filter();
    state_.unreadable_files = catalog_.get_unreadable_file_count();
    state_.directory_exists = catalog_.directory_exists();
    state_.error = catalog_.get_selected_error();
    auto const* const record{catalog_.get_selected_record()};
    if (!record) {
        return;
    }
    auto const level{FTelemetryRunSummary{.map_name = record->metadata.map_name,
                                          .level_id = record->metadata.level_id,
                                          .level_display_name = record->metadata.level_display_name}
                         .level_label()};
    state_.header =
        FText::FromString(FString::Printf(TEXT("%s\nLAUNCHED // %s    END // %s\nREQUESTED // "
                                               "%.3gx    BUILD // %s    PLATFORM // %s\nRUN // %s"),
                                          *level,
                                          *record->metadata.launched_utc,
                                          LexToSerializedString(record->completion.reason),
                                          record->metadata.initial_requested_time_scale,
                                          *record->metadata.environment.build_configuration,
                                          *record->metadata.environment.platform,
                                          *record->metadata.run_id));
    state_.analysis = analyze_level_telemetry_run(*record);
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
}

void UTelemetryDashboardWidget::publish() {
    if (view_.IsValid()) {
        view_->replace_state(state_);
    }
}
} // namespace ml::ioj
