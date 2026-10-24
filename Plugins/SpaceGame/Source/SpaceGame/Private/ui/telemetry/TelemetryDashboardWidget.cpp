#include "SpaceGame/ui/telemetry/TelemetryDashboardWidget.h"

#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include "STelemetryDashboardView.h"
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

/* **************************************** */
// State publication
/* **************************************** */
void UTelemetryDashboardWidget::rebuild_state() {
    state_ = FTelemetryDashboardViewState{};
    state_.runs = catalog_.get_runs();
    state_.level_filters = catalog_.get_level_filters();
    state_.baseline_runs = catalog_.get_baseline_candidates();
    state_.selected_baseline_run_id = catalog_.get_baseline_run_id();
    state_.selected_run_id = catalog_.get_selected_run_id();
    state_.selected_level_filter = catalog_.get_level_filter();
    state_.unreadable_files = catalog_.get_unreadable_file_count();
    state_.error = catalog_.get_selected_error();
    if (!external_error_.IsEmpty()) {
        state_.error =
            state_.error.IsEmpty() ? external_error_ : external_error_ + TEXT("\n") + state_.error;
    }
    auto const* const record{catalog_.get_selected_record()};
    if (!record) {
        return;
    }
    auto const level{FTelemetryRunSummary{
        .map_name = UTF8_TO_TCHAR(record->run.metadata.map_name.c_str()),
        .level_id = FName{UTF8_TO_TCHAR(record->run.metadata.level_id.c_str())},
        .level_display_name = UTF8_TO_TCHAR(record->run.metadata.level_display_name.c_str())}
                         .level_label()};
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
                               record->run.metadata.requested_duration_seconds->get_seconds())
            : TEXT("unlimited"),
        UTF8_TO_TCHAR(record->run.metadata.run_id.c_str())));
    state_.analysis = analyze_level_telemetry_run(*record);
    auto const* const baseline{catalog_.get_baseline_record()};
    if (baseline) {
        state_.baseline_analysis = analyze_level_telemetry_run(*baseline);
        TArray<FString> warnings;
        if (baseline->run.metadata.level_source != record->run.metadata.level_source) {
            warnings.Add(TEXT("different level source"));
        }
        if (baseline->run.metadata.settings_source != record->run.metadata.settings_source) {
            warnings.Add(TEXT("different settings source"));
        }
        if (baseline->presentation_enabled != record->presentation_enabled) {
            warnings.Add(TEXT("different presentation modes"));
        }
        if (!warnings.IsEmpty()) {
            state_.compatibility_warning = FText::FromString(TEXT("BASELINE COMPATIBILITY // ") +
                                                             FString::Join(warnings, TEXT(", ")));
        }
    }
    auto last_int = [](sim::LevelTelemetryTickSeries::Int32Data const& series) {
        return series.is_empty() ? 0 : series.last_value();
    };
    auto summary{FString::Printf(
        TEXT(
            "COMPLETED TICKS  //  %llu\nSIMULATED TIME  //  %.3fs\nFINAL FORCES  //  "
            "ENTITIES %d  LASERS %d\nFINAL COUNTERS  //  SPAWN %d  DESTROY %d  KILLS %d  FIRED %d"),
        record->run.completion.completed_ticks,
        static_cast<double>(record->run.completion.completed_ticks) /
            record->run.tick_rate_hz.get_hz(),
        last_int(record->run.tick_series.active_entities),
        last_int(record->run.tick_series.active_lasers),
        last_int(record->run.tick_series.spawned_entities),
        last_int(record->run.tick_series.destroyed_entities),
        last_int(record->run.tick_series.kills),
        last_int(record->run.tick_series.lasers_fired))};
    if (record->run.completion.reason != sim::LevelTelemetryRunEndReason::MissionSucceeded &&
        record->run.completion.reason != sim::LevelTelemetryRunEndReason::MissionFailed) {
        summary += TEXT("\nMISSION  //  INCOMPLETE");
    }
    if (record->run.battle_samples.empty()) {
        summary += TEXT("\nCOMBAT DETAILS  //  UNAVAILABLE");
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
        auto const& combat{record->run.battle_samples.back().combat};
        summary +=
            FString::Printf(TEXT("\nBATTLE SAMPLES // %d\nCOMBAT  //  SHOTS %llu  HITS %llu  "
                                 "DAMAGE %.0f  KILLS %llu  LOSSES %llu"),
                            static_cast<int32>(record->run.battle_samples.size()),
                            sum_counts(combat.shots),
                            sum_counts(combat.hits),
                            sum_counts(combat.damage_dealt),
                            sum_counts(combat.kills),
                            sum_counts(combat.losses));
    }
    state_.summary = FText::FromString(MoveTemp(summary));
}

void UTelemetryDashboardWidget::publish() {
    if (view_.IsValid()) {
        view_->replace_state(state_);
    }
}
} // namespace ioj
