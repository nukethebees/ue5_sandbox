#pragma once

#include "SpaceGame/telemetry/TelemetryDashboardModel.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Blueprint/UserWidget.h>

#include "TelemetryDashboardWidget.generated.h"

namespace ml::ioj {
class STelemetryDashboardView;
class UGameSubsystem;

struct FTelemetryDashboardViewState {
    TArray<FTelemetryRunSummary> runs{};
    TArray<FString> level_filters{};
    TArray<FTelemetryRunSummary> baseline_runs{};
    FString selected_run_id{};
    FString selected_level_filter{TEXT("All levels")};
    int32 unreadable_files{};
    bool directory_exists{};
    FString error{};
    FText header{};
    FText summary{};
    FTelemetryRunAnalysis analysis{};
    FTelemetryRunAnalysis baseline_analysis{};
    FString selected_baseline_run_id{};
    FText compatibility_warning{};
};

UCLASS()
class SPACEGAME_API UTelemetryDashboardWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UTelemetryDashboardWidget(FObjectInitializer const& object_initializer);

    void refresh();
    bool select_run(FString const& run_id);
    void select_level_filter(FString const& level_label);
    void select_baseline(FString const& run_id);
    void focus_primary_action();
    void set_external_error(FString error);

    [[nodiscard]] auto get_catalog() const -> FTelemetryRunCatalog const& { return catalog_; }
    [[nodiscard]] auto get_view_state() const -> FTelemetryDashboardViewState const& {
        return state_;
    }
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void handle_run_selected(FString run_id);
    void handle_level_filter_selected(FString level_label);
    void handle_baseline_selected(FString run_id);
    void rebuild_state();
    void publish();

    FTelemetryRunCatalog catalog_{};
    FTelemetryDashboardViewState state_{};
    TSharedPtr<STelemetryDashboardView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    FString external_error_{};
};
} // namespace ml::ioj
