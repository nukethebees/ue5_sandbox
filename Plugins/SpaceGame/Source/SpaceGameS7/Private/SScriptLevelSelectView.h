#pragma once

#include <SpaceGamePresentation/ui/style/GameUiStyle.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <Widgets/SCompoundWidget.h>

class SEditableText;
class SCheckBox;

namespace ioj {
class SGameButton;
}
namespace SlateGenerated::ioj::s7 {
struct SScriptLevelSelectViewBuilder;
}

namespace ioj::s7 {
using ioj::levels::authoring::LevelCatalogCategory;

class SScriptLevelSelectView final : public SCompoundWidget {
    friend struct ::SlateGenerated::ioj::s7::SScriptLevelSelectViewBuilder;
  public:
    SLATE_BEGIN_ARGS(SScriptLevelSelectView)
        : _Style(nullptr) {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void replace_catalog(FLevelSelectViewState const& state);
    void present(MenuUpdate const& update);
    void update_state(FLevelSelectViewState const& state);

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    auto build_header() -> TSharedRef<SWidget>;
    auto build_catalog() -> TSharedRef<SWidget>;
    auto build_details() -> TSharedRef<SWidget>;
    auto catalog_caption() const -> FText;
    auto catalog_title() const -> FText;
    auto directive_title() const -> FText;
    auto launch_text() const -> FText;
    auto handle_category_selected(LevelCatalogCategory category) -> FReply;
    auto handle_level_selected(int32 button_index) -> FReply;
    auto request_launch() -> FReply;
    void handle_battle_speed_changed(FText const& text);
    void handle_battle_duration_changed(FText const& text);
    void handle_simulation_only_changed(ECheckBoxState state);
    void update_launch_availability();
    void rebuild_catalog(FLevelSelectViewState const& state);
    void update_selection(int32 button_index);
    void update_category_selection();
    void focus_level(int32 button_index);

    FGameUiStyle const* style_{};

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
    TArray<std::string> level_ids_;
    TSharedPtr<SCheckBox> simulation_only_;
    TSharedPtr<SGameButton> launch_button_{};
    TSharedPtr<SGameButton> level_category_button_{};
    TSharedPtr<SGameButton> battle_viewer_category_button_{};
    TSharedPtr<SGameButton> benchmark_category_button_{};
    TArray<TSharedPtr<SGameButton>> level_buttons_{};
    LevelCatalogCategory category_{LevelCatalogCategory::Playable};
    int32 selected_button_index_{INDEX_NONE};
    bool selected_level_can_launch_{};
};
} // namespace ioj::s7
