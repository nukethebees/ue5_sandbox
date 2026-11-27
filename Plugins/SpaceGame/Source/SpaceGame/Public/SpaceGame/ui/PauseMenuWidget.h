#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include <ioj/host/pause_menu.h>

#include "PauseMenuWidget.generated.h"

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
    void prepare_for_open();
    void present(host::PauseCommand command);

    FPauseReturnToLevelSelectRequested return_to_level_select_requested;
    FPauseQuitRequested quit_requested;
  protected:
    /* **************************************** */
    // Widget lifecycle
    /* **************************************** */
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    auto focus_target() const -> UWidget* override;

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
    auto button_for(host::PauseMenuItem item) const -> UMenuButtonWidget*;
    void send_event(host::PauseWidgetEvent event);
    void handle_options_modal_state_changed(bool visible);
    void set_navigation_enabled(bool enabled);
    void apply_ui_style();

    /* **************************************** */
    // State
    /* **************************************** */
    bool options_modal_visible_{};
};
}
