#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"

#include <Input/UIActionBindingHandle.h>

#include "PauseMenuWidget.generated.h"

class UInputAction;
class UOverlay;
class UTextBlock;

namespace ioj {
class UMenuButtonWidget;

DECLARE_MULTICAST_DELEGATE(FPauseReturnToLevelSelectRequested);
DECLARE_MULTICAST_DELEGATE(FPauseQuitRequested);

UCLASS()
class SPACEGAME_API UPauseMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    /* **************************************** */
    // State and opening
    /* **************************************** */
    [[nodiscard]] auto get_options_widget() const -> UOptionsWidget* { return options_widget; }
    void prepare_for_open(UInputAction& toggle_action);

    FPauseReturnToLevelSelectRequested return_to_level_select_requested;
    FPauseQuitRequested quit_requested;
  protected:
    /* **************************************** */
    // Widget lifecycle
    /* **************************************** */
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    void NativeOnActivated() override;
    void NativeOnDeactivated() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    auto NativeOnHandleBackAction() -> bool override;

    /* **************************************** */
    // Bound widgets
    /* **************************************** */
    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* resume_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* audio_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* controls_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* return_to_level_select_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* quit_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* page_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* paused_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UOptionsWidget* options_widget{nullptr};

  private:
    /* **************************************** */
    // Navigation callbacks
    /* **************************************** */
    void handle_resume();
    void handle_audio();
    void handle_controls();
    void handle_return_to_level_select();
    void handle_quit();
    void handle_toggle_action();

    /* **************************************** */
    // View state and presentation
    /* **************************************** */
    void show_options_tab(EOptionsTab tab);
    void request_close(FSimpleDelegate continuation);
    void complete_resume();
    void complete_return_to_level_select();
    void complete_quit();
    void handle_options_modal_state_changed(bool visible);
    void set_navigation_enabled(bool enabled);
    void apply_ui_style();

    /* **************************************** */
    // State
    /* **************************************** */
    TWeakObjectPtr<UInputAction> toggle_action_;
    FUIActionBindingHandle toggle_action_binding_;
    bool options_modal_visible_{};
    bool terminal_action_requested_{false};
};
}
