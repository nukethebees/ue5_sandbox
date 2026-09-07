#pragma once

#include "SpaceGame/missions/TestMissionState.h"
#include "SpaceGame/simulation/LevelTelemetrySnapshot.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Widgets/SCompoundWidget.h>

class SGraphPlot;
class STextBlock;

namespace ml::ioj {
class SGameButton;

enum class ELevelCompletionAction : uint8 {
    ReturnToMissionControl,
    KeepOperating,
};

class SLevelCompletionView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SLevelCompletionView)
        : _Style(nullptr) {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_EVENT(FSimpleDelegate, OnReturnToMissionControl)
    SLATE_EVENT(FSimpleDelegate, OnKeepOperating)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void update_report(FString const& level_display_name,
                       ETestMissionState state,
                       FLevelTelemetrySnapshot const& snapshot);
    void focus_primary_action();

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
  private:
    auto build_header() const -> TSharedRef<SWidget>;
    auto build_summary() -> TSharedRef<SWidget>;
    auto build_report() -> TSharedRef<SWidget>;
    auto build_footer() -> TSharedRef<SWidget>;
    auto build_metric(FText label, TSharedPtr<STextBlock>& value) const -> TSharedRef<SWidget>;
    auto build_detail(FText label, TSharedPtr<STextBlock>& value) const -> TSharedRef<SWidget>;
    auto activate_action(ELevelCompletionAction action, FSimpleDelegate delegate) -> FReply;
    void focus_action(ELevelCompletionAction action);

    FGameUiStyle const* style_{};
    FSimpleDelegate on_return_to_mission_control_{};
    FSimpleDelegate on_keep_operating_{};
    ELevelCompletionAction focused_action_{ELevelCompletionAction::ReturnToMissionControl};

    TSharedPtr<STextBlock> mission_name_{};
    TSharedPtr<STextBlock> mission_result_{};
    TSharedPtr<STextBlock> objective_status_{};
    TSharedPtr<STextBlock> elapsed_time_{};
    TSharedPtr<STextBlock> kills_{};
    TSharedPtr<STextBlock> destroyed_entities_{};
    TSharedPtr<STextBlock> lasers_fired_{};
    TSharedPtr<STextBlock> spawned_entities_{};
    TSharedPtr<STextBlock> active_entities_{};
    TSharedPtr<STextBlock> active_lasers_{};
    TSharedPtr<SGraphPlot> activity_graph_{};
    TArray<TSharedPtr<SGameButton>> action_buttons_{};
};
} // namespace ml::ioj
