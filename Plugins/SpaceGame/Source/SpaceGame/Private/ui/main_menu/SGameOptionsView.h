#pragma once

#include "ioj/settings/game_settings.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <SpaceGame/settings/ControlBindingView.h>

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
class UOptionsDraft;
struct SystemCapabilities;

using FControlsFocusIdentity = std::variant<GameSettingDevice, FlightMapping>;

struct FControlsFocusTarget {
    FControlsFocusIdentity identity{};
    TFunction<void()> focus{};
    TFunction<bool()> has_focus{};
};

DECLARE_DELEGATE_OneParam(FOnOptionsInteractionModalChanged, bool);

class SGameOptionsView final : public SCompoundWidget {
    friend struct ::SlateGenerated::ioj::SGameOptionsViewBuilder;
  public:
    SLATE_BEGIN_ARGS(SGameOptionsView)
        : _Settings(nullptr)
        , _Capabilities(nullptr)
        , _Style(nullptr)
        , _Page(EMenuPage::SelectLevel)
        , _Audio() {}
    SLATE_ARGUMENT(UOptionsDraft*, Settings)
    SLATE_ARGUMENT(SystemCapabilities const*, Capabilities)
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ATTRIBUTE(EMenuPage, Page)
    SLATE_ARGUMENT(FGameAudioFacade, Audio)
    SLATE_EVENT(FSimpleDelegate, OnApply)
    SLATE_EVENT(FSimpleDelegate, OnReset)
    SLATE_EVENT(FSimpleDelegate, OnDirtyApply)
    SLATE_EVENT(FSimpleDelegate, OnDirtyDiscard)
    SLATE_EVENT(FSimpleDelegate, OnDirtyStay)
    SLATE_EVENT(FSimpleDelegate, OnConfirmDisplay)
    SLATE_EVENT(FSimpleDelegate, OnRevertDisplay)
    SLATE_EVENT(FOnOptionsInteractionModalChanged, OnInteractionModalChanged)
    SLATE_END_ARGS()

    /* **************************************** */
    // Lifecycle and state
    /* **************************************** */
    void Construct(FArguments const& args);
    void show_page();
    void refresh();
    void refresh_controls();
    void focus_content();
    void show_dirty_prompt();
    void hide_dirty_prompt();
    auto is_dirty_prompt_visible() const -> bool;
    auto dismiss_interaction_modal() -> bool;

    /* **************************************** */
    // Input handling
    /* **************************************** */
    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
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
    void rebuild_controls_page(TOptional<FControlsFocusIdentity> requested_focus = {},
                               bool reset_scroll = false);
    void request_controls_rebuild(TOptional<FControlsFocusIdentity> requested_focus,
                                  bool reset_scroll = false);
    auto handle_deferred_controls_rebuild(double current_time, float delta_time)
        -> EActiveTimerReturnType;
    auto handle_deferred_controls_restore(double current_time, float delta_time)
        -> EActiveTimerReturnType;
    auto build_binding_button(FControlBindingView const& binding) -> TSharedRef<SWidget>;
    void open_binding_management(FControlBindingView const& binding);
    void close_binding_management(bool restore_focus);
    void begin_binding_capture(FControlBindingView const& binding);
    void handle_capture_key(PhysicalKey key);
    void accept_binding_key(PhysicalKey key);
    void close_binding_prompt(bool restore_focus);
    void complete_binding_change();

    /* **************************************** */
    // Settings and prompts
    /* **************************************** */
    auto build_setting_row(GameSettingDescriptor const& descriptor, TFunction<void()>& focus_action)
        -> TSharedRef<SWidget>;
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
    auto controls_focus_identity() const -> TOptional<FControlsFocusIdentity>;
    void register_controls_focus(FControlsFocusIdentity identity,
                                 TSharedRef<SGameButton> const& button);
    void restore_controls_focus(FControlsFocusIdentity const& identity);
    void remember_focus();
    void restore_focus();

    /* **************************************** */
    // State
    /* **************************************** */
    TWeakObjectPtr<UOptionsDraft> settings_{};
    SystemCapabilities const* capabilities_{};
    FGameUiStyle const* style_{};
    FGameAudioFacade audio_{};
    TAttribute<EMenuPage> page_;

    FSimpleDelegate on_apply_{};
    FSimpleDelegate on_reset_{};
    FSimpleDelegate on_dirty_apply_{};
    FSimpleDelegate on_dirty_discard_{};
    FSimpleDelegate on_dirty_stay_{};
    FSimpleDelegate on_confirm_display_{};
    FSimpleDelegate on_revert_display_{};
    FOnOptionsInteractionModalChanged on_interaction_modal_changed_{};

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
    TOptional<FControlBindingView> managed_binding_{};
    TOptional<FlightMapping> captured_binding_{};
    PhysicalKey captured_key_{};
    FText capture_error_{};
    TArray<FControlsFocusTarget> controls_focus_targets_{};
    TOptional<FControlsFocusIdentity> pending_controls_focus_{};
    TOptional<FControlsFocusIdentity> pending_controls_rebuild_focus_{};
    float pending_controls_scroll_offset_{};
    bool pending_controls_reset_scroll_{};
    bool controls_rebuild_pending_{};
    TWeakPtr<SWidget> previous_focus_{};
    TFunction<void()> focus_page_;
    bool dirty_prompt_visible_{};
    bool display_prompt_visible_{};
    bool binding_management_visible_{};
    GameSettingDevice controls_device_{GameSettingDevice::KeyboardMouse};
};

} // namespace ioj
