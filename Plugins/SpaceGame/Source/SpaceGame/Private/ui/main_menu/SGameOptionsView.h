#pragma once

#include "ioj/settings/game_settings.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <ioj/menu/player_menu.h>

#include <Widgets/SCompoundWidget.h>

#include <variant>

class SBox;
class SVerticalBox;
class SScrollBox;

namespace SlateGenerated::ioj {
struct SGameOptionsViewBuilder;
}

namespace ioj {
class SGameButton;
struct SystemCapabilities;

struct FControlsFocusTarget {
    MenuTarget identity{GameSettingDevice::KeyboardMouse};
    TSharedPtr<SGameButton> button;
};

class SGameOptionsView final : public SCompoundWidget {
    enum class PendingRebuild { None, PreserveScroll, ResetScroll };
    friend struct ::SlateGenerated::ioj::SGameOptionsViewBuilder;
  public:
    SLATE_BEGIN_ARGS(SGameOptionsView)
        : _Capabilities(nullptr)
        , _Style(nullptr)
        , _Page(MainMenuItem::SelectLevel)
        , _Audio() {}
    SLATE_ARGUMENT(SystemCapabilities const*, Capabilities)
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(MainMenuItem, Page)
    SLATE_ARGUMENT(FGameAudioFacade, Audio)
    SLATE_END_ARGS()

    /* **************************************** */
    // Lifecycle and state
    /* **************************************** */
    void Construct(FArguments const& args);
    void show_page(MainMenuItem page);
    void refresh();
    void refresh_controls();
    void present_binding_prompt();
    void present(MenuUpdate const& update);
    void focus_content();

    /* **************************************** */
    // Input handling
    /* **************************************** */
    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    /* **************************************** */
    // Layout construction
    /* **************************************** */
    auto build_header() -> TSharedRef<SWidget>;
    auto build_body() -> TSharedRef<SWidget>;
    auto build_footer() -> TSharedRef<SWidget>;
    auto build_category_page(GameSettingCategory category) -> TSharedRef<SWidget>;
    auto build_controls_page() -> TSharedRef<SWidget>;

    /* **************************************** */
    // Control bindings
    /* **************************************** */
    void rebuild_controls_page(bool reset_scroll = false);
    void request_controls_rebuild(PendingRebuild request = PendingRebuild::PreserveScroll);
    auto handle_deferred_controls_rebuild(double current_time, float delta_time)
        -> EActiveTimerReturnType;
    auto handle_deferred_controls_restore(double current_time, float delta_time)
        -> EActiveTimerReturnType;
    auto build_binding_button(ControlBindingRow const& binding) -> TSharedRef<SWidget>;

    /* **************************************** */
    // Settings and prompts
    /* **************************************** */
    auto build_setting_row(GameSettingDescriptor const& descriptor) -> TSharedRef<SWidget>;
    auto build_system_page() -> TSharedRef<SWidget>;
    auto build_dirty_prompt() -> TSharedRef<SWidget>;
    auto build_display_prompt() -> TSharedRef<SWidget>;
    auto build_binding_management_prompt() -> TSharedRef<SWidget>;
    auto build_capture_prompt() -> TSharedRef<SWidget>;
    auto build_conflict_prompt() -> TSharedRef<SWidget>;
    auto build_modal(TAttribute<FText> title, TArray<TSharedRef<SGameButton>> const& buttons)
        -> TSharedRef<SWidget>;

    /* **************************************** */
    // Formatting and focus helpers
    /* **************************************** */
    auto setting_float(GameSetting setting) const -> float;
    auto format_range_value(GameSettingDescriptor const& descriptor) const -> FText;
    void register_controls_focus(MenuTarget identity, TSharedRef<SGameButton> const& button);
    void restore_controls_focus(MenuTarget const& identity);
    void restore_focus();

    /* **************************************** */
    // State
    /* **************************************** */
    SystemCapabilities const* capabilities_{};
    FGameUiStyle const* style_{};
    FGameAudioFacade audio_{};
    PendingRebuild pending_rebuild_{};
    MainMenuItem rendered_page_{};

    TSharedPtr<SGameButton> apply_button_{};
    TSharedPtr<SGameButton> reset_button_{};
    TSharedPtr<SGameButton> dirty_apply_button_{};
    TSharedPtr<SGameButton> dirty_discard_button_{};
    TSharedPtr<SGameButton> dirty_stay_button_{};
    TSharedPtr<SGameButton> confirm_display_button_{};
    TSharedPtr<SGameButton> revert_display_button_{};
    TSharedPtr<SGameButton> conflict_replace_button_{};
    TSharedPtr<SGameButton> conflict_cancel_button_{};
    TSharedPtr<SGameButton> binding_change_button_{};
    TSharedPtr<SGameButton> binding_clear_button_{};
    TSharedPtr<SGameButton> binding_reset_button_{};
    TSharedPtr<SGameButton> binding_cancel_button_{};
    TSharedPtr<SBox> page_content_;
    TSharedPtr<SWidget> dirty_prompt_{};
    TSharedPtr<SWidget> display_prompt_{};
    TSharedPtr<SWidget> binding_management_prompt_{};
    TSharedPtr<SWidget> capture_prompt_{};
    TSharedPtr<SWidget> conflict_prompt_{};
    TSharedPtr<SVerticalBox> controls_content_{};
    TSharedPtr<SScrollBox> controls_scroll_box_{};
    TArray<FControlsFocusTarget> controls_focus_targets_{};
    float pending_controls_scroll_offset_{};
    TMap<GameSetting, TFunction<void()>> setting_focus_;
};

} // namespace ioj
