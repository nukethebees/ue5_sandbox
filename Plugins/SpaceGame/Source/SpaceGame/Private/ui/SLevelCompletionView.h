#pragma once

#include "SpaceGame/levels/LevelOutcome.h"
#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <SpaceGame/levels/LevelCompletion.h>

#include <Widgets/SCompoundWidget.h>

class STextBlock;

namespace ioj {
class SGameButton;

class SLevelCompletionView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SLevelCompletionView)
        : _Style(nullptr)
        , _Audio() {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(FGameAudioFacade, Audio)
    SLATE_EVENT(FSimpleDelegate, OnReturnToMissionControl)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void set_completion(FLevelCompletion const& completion);
    void focus_primary_action();

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    auto build_header() const -> TSharedRef<SWidget>;
    auto build_summary() -> TSharedRef<SWidget>;
    auto build_report() -> TSharedRef<SWidget>;
    auto build_footer() -> TSharedRef<SWidget>;
    auto build_metric(FText label, TSharedPtr<STextBlock>& value) const -> TSharedRef<SWidget>;
    auto activate_action(FSimpleDelegate delegate) -> FReply;

    FGameUiStyle const* style_{};
    FGameAudioFacade audio_{};
    FSimpleDelegate on_return_to_mission_control_{};

    TSharedPtr<STextBlock> mission_name_{};
    TSharedPtr<STextBlock> mission_result_{};
    TSharedPtr<STextBlock> failure_reason_{};
    TSharedPtr<STextBlock> objective_status_{};
    TSharedPtr<STextBlock> new_best_time_{};
    TSharedPtr<STextBlock> elapsed_time_{};
    TSharedPtr<STextBlock> kills_{};
    TSharedPtr<SGameButton> return_button_;
};
} // namespace ioj
