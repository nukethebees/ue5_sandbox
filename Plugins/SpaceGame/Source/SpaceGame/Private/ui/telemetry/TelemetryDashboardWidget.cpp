#include "SpaceGame/ui/telemetry/TelemetryDashboardWidget.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>

#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include "STelemetryDashboardView.h"

#include <SandboxCoreEngine/strings.h>

#include <Engine/GameInstance.h>

namespace ml::ioj {
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
    auto const level{FTelemetryRunSummary{
        .map_name = UTF8_TO_TCHAR(record->metadata.map_name.c_str()),
        .level_id = FName{UTF8_TO_TCHAR(record->metadata.level_id.c_str())},
        .level_display_name = UTF8_TO_TCHAR(record->metadata.level_display_name.c_str())}
                         .level_label()};
    auto const completion_reason{
        ml::to_fstring(::ioj::sim::to_serialized_string(record->completion.reason))};
    state_.header = FText::FromString(FString::Printf(
        TEXT("%s\nLAUNCHED // %s    END // %s\nREQUESTED // "
             "%.3gx    MODE // %s    DURATION // %s\nBUILD // %s    "
             "PLATFORM // %s\nSOURCE SHA-256 // %s\nRUN // %s"),
        *level,
        UTF8_TO_TCHAR(record->metadata.launched_utc.c_str()),
        *completion_reason,
        record->metadata.initial_requested_time_scale,
        *record->metadata.presentation_mode,
        record->metadata.requested_duration_seconds.has_value()
            ? *FString::Printf(TEXT("%.3fs"), record->metadata.requested_duration_seconds.value())
            : TEXT("unlimited"),
        *record->metadata.environment.build_configuration,
        *record->metadata.environment.platform,
        record->metadata.source_sha256.empty()
            ? TEXT("unavailable")
            : UTF8_TO_TCHAR(record->metadata.source_sha256.c_str()),
        UTF8_TO_TCHAR(record->metadata.run_id.c_str())));
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
    auto last_int = [](::ioj::sim::LevelTelemetryTickSeries::Int32Data const& series) {
        return series.is_empty() ? 0 : series.last_value();
    };
    auto summary{FString::Printf(
        TEXT(
            "COMPLETED TICKS  //  %llu\nSIMULATED TIME  //  %.3fs\nFINAL FORCES  //  "
            "ENTITIES %d  LASERS %d\nFINAL COUNTERS  //  SPAWN %d  DESTROY %d  KILLS %d  FIRED %d"),
        record->completion.completed_ticks,
        record->completion.simulated_elapsed_seconds,
        last_int(record->tick_series.active_entities),
        last_int(record->tick_series.active_lasers),
        last_int(record->tick_series.spawned_entities),
        last_int(record->tick_series.destroyed_entities),
        last_int(record->tick_series.kills),
        last_int(record->tick_series.lasers_fired))};
    if (record->battle_samples.IsEmpty()) {
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
        auto const& combat{record->battle_samples.Last().combat};
        auto const winner{record->completion.reason ==
                                  ::ioj::sim::LevelTelemetryRunEndReason::BattleResolved
                              ? (record->completion.winning_team.has_value()
                                     ? ml::to_fstring(::ioj::sim::to_serialized_string(
                                           record->completion.winning_team.value()))
                                     : FString{TEXT("draw")})
                              : FString{TEXT("—")}};
        summary += FString::Printf(
            TEXT("\nBATTLE RESULT  //  %s    SAMPLES // %d\nCOMBAT  //  SHOTS %llu  HITS %llu  "
                 "DAMAGE %.0f  KILLS %llu  LOSSES %llu"),
            *winner,
            record->battle_samples.Num(),
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
} // namespace ml::ioj
