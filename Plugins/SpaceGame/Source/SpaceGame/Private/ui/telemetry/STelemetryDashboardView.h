#pragma once

#include "SpaceGame/ui/telemetry/TelemetryDashboardWidget.h"

#include <Widgets/SCompoundWidget.h>

class SButton;
class SScrollBox;

namespace ml::ioj {
DECLARE_DELEGATE_OneParam(FOnTelemetryRunSelected, FString);
DECLARE_DELEGATE_OneParam(FOnTelemetryLevelFilterSelected, FString);
DECLARE_DELEGATE_OneParam(FOnTelemetryBaselineSelected, FString);

enum class ETelemetryDashboardSection : uint8 {
    Overview,
    Timing,
    Workload,
    Activity,
    Queries,
};

class STelemetryDashboardView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(STelemetryDashboardView)
        : _Style(nullptr) {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_EVENT(FSimpleDelegate, OnRefresh)
    SLATE_EVENT(FOnTelemetryRunSelected, OnRunSelected)
    SLATE_EVENT(FOnTelemetryLevelFilterSelected, OnLevelFilterSelected)
    SLATE_EVENT(FOnTelemetryBaselineSelected, OnBaselineSelected)
    SLATE_END_ARGS()

    /* **************************************** */
    // Lifecycle and state
    /* **************************************** */
    void Construct(FArguments const& args);
    void replace_state(FTelemetryDashboardViewState const& state);
    void focus_primary_action();
    auto SupportsKeyboardFocus() const -> bool override { return true; }
  private:
    /* **************************************** */
    // Layout construction
    /* **************************************** */
    auto build() -> TSharedRef<SWidget>;
    auto build_sidebar() -> TSharedRef<SWidget>;
    auto build_detail() -> TSharedRef<SWidget>;

    /* **************************************** */
    // Actions
    /* **************************************** */
    auto handle_refresh() -> FReply;
    auto handle_filter() -> FReply;
    auto handle_baseline() -> FReply;
    auto handle_run(FString run_id) -> FReply;
    auto handle_section(ETelemetryDashboardSection section) -> FReply;

    /* **************************************** */
    // State
    /* **************************************** */
    FGameUiStyle const* style_{};
    FTelemetryDashboardViewState state_{};
    FSimpleDelegate on_refresh_{};
    FOnTelemetryRunSelected on_run_selected_{};
    FOnTelemetryLevelFilterSelected on_level_filter_selected_{};
    FOnTelemetryBaselineSelected on_baseline_selected_{};
    TSharedPtr<SButton> primary_button_{};
    TSharedPtr<SScrollBox> detail_scroll_{};
    TSharedPtr<SWidget> timing_section_{};
    TSharedPtr<SWidget> workload_section_{};
    TSharedPtr<SWidget> activity_section_{};
    TSharedPtr<SWidget> queries_section_{};
};
} // namespace ml::ioj
