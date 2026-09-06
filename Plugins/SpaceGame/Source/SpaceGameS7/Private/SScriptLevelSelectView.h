#pragma once

#include <SpaceGame/ui/style/GameUiStyle.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <Widgets/SCompoundWidget.h>

namespace ml::ioj {
class SGameButton;
}

namespace ml::s7 {
DECLARE_DELEGATE_OneParam(FOnLevelRowSelected, int32);

class SScriptLevelSelectView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SScriptLevelSelectView)
        : _Style(nullptr) {}
    SLATE_ARGUMENT(ml::ioj::FGameUiStyle const*, Style)
    SLATE_EVENT(FOnLevelRowSelected, OnLevelSelected)
    SLATE_EVENT(FSimpleDelegate, OnRefresh)
    SLATE_EVENT(FSimpleDelegate, OnLaunch)
    SLATE_EVENT(FSimpleDelegate, OnStartPaused)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void replace_catalog(FLevelSelectViewState const& state);
    void update_state(FLevelSelectViewState const& state);
    void focus_selected_level();

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
  private:
    auto build_header() const -> TSharedRef<SWidget>;
    auto build_catalog() -> TSharedRef<SWidget>;
    auto build_details() -> TSharedRef<SWidget>;
    auto build_footer() -> TSharedRef<SWidget>;
    auto handle_level_selected(int32 button_index) -> FReply;
    auto handle_action(FSimpleDelegate delegate) -> FReply;
    void rebuild_catalog(FLevelSelectViewState const& state);
    void update_selection(int32 button_index);
    void focus_level(int32 button_index);

    ml::ioj::FGameUiStyle const* style_{};
    FOnLevelRowSelected on_level_selected_{};
    FSimpleDelegate on_refresh_{};
    FSimpleDelegate on_launch_{};
    FSimpleDelegate on_start_paused_{};

    TSharedPtr<SVerticalBox> catalog_rows_{};
    TSharedPtr<STextBlock> title_{};
    TSharedPtr<STextBlock> status_{};
    TSharedPtr<STextBlock> description_{};
    TSharedPtr<STextBlock> filename_{};
    TSharedPtr<STextBlock> details_{};
    TSharedPtr<STextBlock> script_{};
    TSharedPtr<ml::ioj::SGameButton> launch_button_{};
    TSharedPtr<ml::ioj::SGameButton> start_paused_button_{};
    TArray<TSharedPtr<ml::ioj::SGameButton>> level_buttons_{};
    int32 selected_button_index_{INDEX_NONE};
};
} // namespace ml::s7
