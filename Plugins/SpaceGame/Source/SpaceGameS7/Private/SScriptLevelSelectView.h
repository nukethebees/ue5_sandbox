#pragma once

#include <SpaceGamePresentation/ui/style/GameUiStyle.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <Widgets/SCompoundWidget.h>

class SEditableText;
class SCheckBox;

namespace ml::ioj {
class SGameButton;
}
namespace SlateGenerated::ml::s7 {
struct SScriptLevelSelectViewBuilder;
}

namespace ml::s7 {
DECLARE_DELEGATE_OneParam(FOnLevelRowSelected, int32);
DECLARE_DELEGATE_OneParam(FOnLevelCategorySelected, ELevelCatalogCategory);
DECLARE_DELEGATE_OneParam(FOnBattleSpeedChanged, TOptional<double>);
DECLARE_DELEGATE_TwoParams(FOnBattleDurationChanged, TOptional<double>, bool);
DECLARE_DELEGATE_OneParam(FOnBattleBoolChanged, bool);

class SScriptLevelSelectView final : public SCompoundWidget {
    friend struct ::SlateGenerated::ml::s7::SScriptLevelSelectViewBuilder;
  public:
    SLATE_BEGIN_ARGS(SScriptLevelSelectView)
        : _Style(nullptr) {}
    SLATE_ARGUMENT(ml::ioj::FGameUiStyle const*, Style)
    SLATE_EVENT(FOnLevelRowSelected, OnLevelSelected)
    SLATE_EVENT(FOnLevelCategorySelected, OnCategorySelected)
    SLATE_EVENT(FOnBattleSpeedChanged, OnBattleSpeedChanged)
    SLATE_EVENT(FOnBattleDurationChanged, OnBattleDurationChanged)
    SLATE_EVENT(FOnBattleBoolChanged, OnBattleSimulationOnlyChanged)
    SLATE_EVENT(FOnBattleBoolChanged, OnBattleDetailedTimingChanged)
    SLATE_EVENT(FSimpleDelegate, OnLaunch)
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
    auto build_header() -> TSharedRef<SWidget>;
    auto build_catalog() -> TSharedRef<SWidget>;
    auto build_details() -> TSharedRef<SWidget>;
    auto catalog_caption() const -> FText;
    auto catalog_title() const -> FText;
    auto directive_title() const -> FText;
    auto launch_text() const -> FText;
    auto handle_category_selected(ELevelCatalogCategory category) -> FReply;
    auto handle_level_selected(int32 button_index) -> FReply;
    auto handle_action(FSimpleDelegate delegate) -> FReply;
    void handle_battle_speed_changed(FText const& text);
    void handle_battle_duration_changed(FText const& text);
    void handle_simulation_only_changed(ECheckBoxState state);
    void handle_detailed_timing_changed(ECheckBoxState state);
    void update_launch_availability();
    void rebuild_catalog(FLevelSelectViewState const& state);
    void update_selection(int32 button_index);
    void update_category_selection();
    void focus_active_category();
    void focus_level(int32 button_index);

    ml::ioj::FGameUiStyle const* style_{};
    FOnLevelRowSelected on_level_selected_{};
    FOnLevelCategorySelected on_category_selected_{};
    FOnBattleSpeedChanged on_battle_speed_changed_{};
    FOnBattleDurationChanged on_battle_duration_changed_{};
    FOnBattleBoolChanged on_battle_simulation_only_changed_{};
    FOnBattleBoolChanged on_battle_detailed_timing_changed_{};
    FSimpleDelegate on_launch_{};

    TSharedPtr<SVerticalBox> catalog_rows_{};
    TSharedPtr<STextBlock> title_{};
    TSharedPtr<STextBlock> status_{};
    TSharedPtr<STextBlock> description_{};
    TSharedPtr<STextBlock> filename_{};
    TSharedPtr<STextBlock> details_{};
    TSharedPtr<STextBlock> script_{};
    TSharedPtr<STextBlock> launch_mode_status_{};
    TSharedPtr<SVerticalBox> battle_options_{};
    TSharedPtr<SEditableText> battle_speed_input_{};
    TSharedPtr<STextBlock> battle_speed_error_{};
    TSharedPtr<SEditableText> battle_duration_input_{};
    TSharedPtr<STextBlock> battle_duration_error_{};
    TSharedPtr<ml::ioj::SGameButton> launch_button_{};
    TSharedPtr<ml::ioj::SGameButton> mission_category_button_{};
    TSharedPtr<ml::ioj::SGameButton> battle_viewer_category_button_{};
    TSharedPtr<ml::ioj::SGameButton> benchmark_category_button_{};
    TArray<TSharedPtr<ml::ioj::SGameButton>> level_buttons_{};
    ELevelCatalogCategory category_{ELevelCatalogCategory::Mission};
    int32 selected_button_index_{INDEX_NONE};
    bool selected_level_can_launch_{};
    bool battle_speed_valid_{true};
    bool battle_duration_valid_{true};
    bool battle_simulation_only_{};
};
} // namespace ml::s7
