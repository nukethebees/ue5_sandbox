#pragma once

#include "SpaceGame/settings/GameSettings.generated.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Widgets/SCompoundWidget.h>

class SWidgetSwitcher;

namespace SlateGenerated::ml::ioj {
struct SGameOptionsViewBuilder;
}

namespace ml::ioj {
class SGameButton;
class SHiveNavigationButton;
class UGameSettingsSubsystem;
struct FGameCapabilities;

DECLARE_DELEGATE_OneParam(FOnOptionsTabChanged, EOptionsTab);

class SGameOptionsView final : public SCompoundWidget {
    friend struct ::SlateGenerated::ml::ioj::SGameOptionsViewBuilder;
  public:
    SLATE_BEGIN_ARGS(SGameOptionsView)
        : _Settings(nullptr)
        , _Capabilities(nullptr)
        , _Style(nullptr)
        , _InitialTab(EOptionsTab::Video) {}
    SLATE_ARGUMENT(UGameSettingsSubsystem*, Settings)
    SLATE_ARGUMENT(FGameCapabilities const*, Capabilities)
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(EOptionsTab, InitialTab)
    SLATE_EVENT(FOnOptionsTabChanged, OnTabChanged)
    SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_EVENT(FSimpleDelegate, OnApply)
    SLATE_EVENT(FSimpleDelegate, OnReset)
    SLATE_EVENT(FSimpleDelegate, OnDirtyApply)
    SLATE_EVENT(FSimpleDelegate, OnDirtyDiscard)
    SLATE_EVENT(FSimpleDelegate, OnDirtyStay)
    SLATE_EVENT(FSimpleDelegate, OnConfirmDisplay)
    SLATE_EVENT(FSimpleDelegate, OnRevertDisplay)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void set_active_tab(EOptionsTab tab);
    void refresh();
    void focus_active_tab();
    void show_dirty_prompt();
    void hide_dirty_prompt();
    auto is_dirty_prompt_visible() const -> bool;

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
  private:
    auto build_header() -> TSharedRef<SWidget>;
    auto build_navigation() -> TSharedRef<SWidget>;
    auto build_body() -> TSharedRef<SWidget>;
    auto build_footer() -> TSharedRef<SWidget>;
    auto build_category_page(EGameSettingCategory category) -> TSharedRef<SWidget>;
    auto build_setting_row(FGameSettingDescriptor const& descriptor) -> TSharedRef<SWidget>;
    auto build_system_page() -> TSharedRef<SWidget>;
    auto build_dirty_prompt() -> TSharedRef<SWidget>;
    auto build_display_prompt() -> TSharedRef<SWidget>;
    auto build_modal(TAttribute<FText> title, TArray<TSharedRef<SGameButton>> const& buttons)
        -> TSharedRef<SWidget>;
    auto handle_tab(EOptionsTab tab) -> FReply;
    auto tab_text(EOptionsTab tab) const -> FText;
    auto tab_icon(EOptionsTab tab) const -> EGameUiIcon;
    void cycle_tab(int32 direction);
    auto setting_float(EGameSetting setting) const -> float;
    auto format_range_value(FGameSettingDescriptor const& descriptor) const -> FText;
    auto active_category() const -> TOptional<EGameSettingCategory>;
    void remember_focus();
    void restore_focus();

    TWeakObjectPtr<UGameSettingsSubsystem> settings_{};
    FGameCapabilities const* capabilities_{};
    FGameUiStyle const* style_{};
    EOptionsTab active_tab_{EOptionsTab::Video};

    FOnOptionsTabChanged on_tab_changed_{};
    FSimpleDelegate on_back_{};
    FSimpleDelegate on_apply_{};
    FSimpleDelegate on_reset_{};
    FSimpleDelegate on_dirty_apply_{};
    FSimpleDelegate on_dirty_discard_{};
    FSimpleDelegate on_dirty_stay_{};
    FSimpleDelegate on_confirm_display_{};
    FSimpleDelegate on_revert_display_{};

    TArray<TSharedPtr<SHiveNavigationButton>> tab_buttons_{};
    TSharedPtr<SGameButton> apply_button_{};
    TSharedPtr<SGameButton> reset_button_{};
    TSharedPtr<SGameButton> dirty_apply_button_{};
    TSharedPtr<SGameButton> dirty_discard_button_{};
    TSharedPtr<SGameButton> dirty_stay_button_{};
    TSharedPtr<SGameButton> confirm_display_button_{};
    TSharedPtr<SGameButton> revert_display_button_{};
    TSharedPtr<SWidgetSwitcher> page_switcher_{};
    TSharedPtr<SWidget> dirty_prompt_{};
    TSharedPtr<SWidget> display_prompt_{};
    TWeakPtr<SWidget> previous_focus_{};
    bool dirty_prompt_visible_{};
    bool display_prompt_visible_{};
};

} // namespace ml::ioj
