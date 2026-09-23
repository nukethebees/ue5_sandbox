#pragma once

#include "SpaceGame/settings/ControlSettingsTypes.h"
#include "SpaceGame/settings/GameSettings.generated.h"
#include "SpaceGame/ui/main_menu/ControlChordCapture.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Widgets/SCompoundWidget.h>

class SWidgetSwitcher;
class SVerticalBox;
class SScrollBox;

namespace SlateGenerated::ml::ioj {
struct SGameOptionsViewBuilder;
}

namespace ml::ioj {
class SGameButton;
class UGameSettingsSubsystem;
struct FGameCapabilities;

enum class EControlsFocusKind : uint8 { Device, Scope, Binding };

struct FControlsFocusIdentity {
    EControlsFocusKind kind{EControlsFocusKind::Device};
    EGameSettingDevice device{EGameSettingDevice::Shared};
    EShipControlScope scope{EShipControlScope::General};
    FControlBindingIdentity binding{};

    auto operator==(FControlsFocusIdentity const& other) const -> bool {
        return kind == other.kind && device == other.device && scope == other.scope &&
               binding == other.binding;
    }
};

struct FControlsFocusTarget {
    FControlsFocusIdentity identity{};
    TFunction<void()> focus{};
    TFunction<bool()> has_focus{};
};

DECLARE_DELEGATE_OneParam(FOnOptionsTabChanged, EOptionsTab);
DECLARE_DELEGATE_OneParam(FOnOptionsInteractionModalChanged, bool);

class SGameOptionsView final : public SCompoundWidget {
    friend struct ::SlateGenerated::ml::ioj::SGameOptionsViewBuilder;
  public:
    SLATE_BEGIN_ARGS(SGameOptionsView)
        : _Settings(nullptr)
        , _Capabilities(nullptr)
        , _Style(nullptr)
        , _InitialTab(EOptionsTab::Video)
        , _Audio() {}
    SLATE_ARGUMENT(UGameSettingsSubsystem*, Settings)
    SLATE_ARGUMENT(FGameCapabilities const*, Capabilities)
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(EOptionsTab, InitialTab)
    SLATE_ARGUMENT(FGameAudioFacade, Audio)
    SLATE_EVENT(FOnOptionsTabChanged, OnTabChanged)
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
    void set_active_tab(EOptionsTab tab);
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
    auto OnKeyUp(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
    auto OnAnalogValueChanged(FGeometry const& geometry, FAnalogInputEvent const& analog_event)
        -> FReply override;
    auto OnMouseButtonDown(FGeometry const& geometry, FPointerEvent const& mouse_event)
        -> FReply override;
    auto OnMouseButtonUp(FGeometry const& geometry, FPointerEvent const& mouse_event)
        -> FReply override;
    auto OnMouseWheel(FGeometry const& geometry, FPointerEvent const& mouse_event)
        -> FReply override;
  private:
    /* **************************************** */
    // Layout construction
    /* **************************************** */
    auto build_header() -> TSharedRef<SWidget>;
    auto build_body() -> TSharedRef<SWidget>;
    auto build_footer() -> TSharedRef<SWidget>;
    auto build_category_page(EGameSettingCategory category) -> TSharedRef<SWidget>;
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
    void begin_chord_capture(FControlBindingView const& binding);
    auto accept_binding_key(FKey key) -> FReply;
    auto accept_chord_key(FKey key, bool can_be_held) -> FReply;
    auto release_chord_key(FKey key) -> FReply;
    void clear_chord_capture();
    auto confirm_chord_capture() -> FReply;
    void close_binding_prompt(bool restore_focus);
    void complete_binding_change();

    /* **************************************** */
    // Settings and prompts
    /* **************************************** */
    auto build_setting_row(FGameSettingDescriptor const& descriptor,
                           TFunction<void()>& focus_action) -> TSharedRef<SWidget>;
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
    auto tab_text(EOptionsTab tab) const -> FText;
    auto setting_float(EGameSetting setting) const -> float;
    auto format_range_value(FGameSettingDescriptor const& descriptor) const -> FText;
    auto active_category() const -> TOptional<EGameSettingCategory>;
    auto controls_device_type() const -> EHardwareDevicePrimaryType;
    auto controls_focus_identity() const -> TOptional<FControlsFocusIdentity>;
    auto binding_focus_identity(FControlBindingView const& binding) const -> FControlsFocusIdentity;
    void register_controls_focus(FControlsFocusIdentity identity,
                                 TSharedRef<SGameButton> const& button);
    void restore_controls_focus(FControlsFocusIdentity const& identity);
    void remember_focus();
    void restore_focus();

    /* **************************************** */
    // State
    /* **************************************** */
    TWeakObjectPtr<UGameSettingsSubsystem> settings_{};
    FGameCapabilities const* capabilities_{};
    FGameUiStyle const* style_{};
    FGameAudioFacade audio_{};
    EOptionsTab active_tab_{EOptionsTab::Video};

    FOnOptionsTabChanged on_tab_changed_{};
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
    TSharedPtr<SGameButton> chord_confirm_button_{};
    TSharedPtr<SGameButton> chord_clear_button_{};
    TSharedPtr<SGameButton> chord_cancel_button_{};
    TSharedPtr<SGameButton> binding_change_button_{};
    TSharedPtr<SGameButton> binding_clear_button_{};
    TSharedPtr<SGameButton> binding_reset_button_{};
    TSharedPtr<SGameButton> binding_cancel_button_{};
    TSharedPtr<SWidgetSwitcher> page_switcher_{};
    TSharedPtr<SWidget> dirty_prompt_{};
    TSharedPtr<SWidget> display_prompt_{};
    TSharedPtr<SWidget> binding_management_prompt_{};
    TSharedPtr<SWidget> capture_prompt_{};
    TSharedPtr<SWidget> conflict_prompt_{};
    TSharedPtr<SVerticalBox> controls_content_{};
    TSharedPtr<SScrollBox> controls_scroll_box_{};
    TOptional<FControlBindingView> managed_binding_{};
    TOptional<FControlBindingAddress> captured_binding_{};
    TOptional<FControlChordBindingView> captured_chord_{};
    FControlChordCapture chord_capture_{};
    FKey captured_key_{};
    FText capture_error_{};
    int32 captured_chord_dependent_count_{};
    TArray<FControlsFocusTarget> controls_focus_targets_{};
    TOptional<FControlsFocusIdentity> pending_controls_focus_{};
    TOptional<FControlsFocusIdentity> pending_controls_rebuild_focus_{};
    float pending_controls_scroll_offset_{};
    bool pending_controls_reset_scroll_{};
    bool controls_rebuild_pending_{};
    TWeakPtr<SWidget> previous_focus_{};
    TArray<TFunction<void()>> page_focus_actions_{};
    bool dirty_prompt_visible_{};
    bool display_prompt_visible_{};
    bool binding_management_visible_{};
    EGameSettingDevice controls_device_{EGameSettingDevice::KeyboardMouse};
    EShipControlScope controls_scope_{EShipControlScope::General};
};

} // namespace ml::ioj
