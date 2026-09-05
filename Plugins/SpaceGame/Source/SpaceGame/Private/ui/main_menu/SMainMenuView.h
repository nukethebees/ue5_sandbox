#pragma once

#include "SpaceGame/ui/main_menu/MainMenuLandingWidget.h"

#include <Widgets/SCompoundWidget.h>

namespace ml::ioj {
class SGameButton;

class SMainMenuView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SMainMenuView)
        : _Style(nullptr)
        , _InitialAction(EMainMenuAction::SelectMission) {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(EMainMenuAction, InitialAction)
    SLATE_EVENT(FSimpleDelegate, OnSelectMission)
    SLATE_EVENT(FSimpleDelegate, OnSaveData)
    SLATE_EVENT(FSimpleDelegate, OnOptions)
    SLATE_EVENT(FSimpleDelegate, OnQuitGame)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void focus_action(EMainMenuAction action);

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
  private:
    auto build_header() const -> TSharedRef<SWidget>;
    auto build_navigation() -> TSharedRef<SWidget>;
    auto build_identity() const -> TSharedRef<SWidget>;
    auto build_footer() const -> TSharedRef<SWidget>;
    void add_action(TSharedRef<SVerticalBox> const& actions,
                    EMainMenuAction action,
                    FText text,
                    FSimpleDelegate delegate);
    auto activate_action(EMainMenuAction action, FSimpleDelegate delegate) -> FReply;

    FGameUiStyle const* style_{};
    FSimpleDelegate on_select_mission_{};
    FSimpleDelegate on_save_data_{};
    FSimpleDelegate on_options_{};
    FSimpleDelegate on_quit_game_{};
    TArray<TSharedPtr<SGameButton>> action_buttons_{};
    EMainMenuAction focused_action_{EMainMenuAction::SelectMission};
};
} // namespace ml::ioj
