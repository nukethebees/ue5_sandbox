#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include <Input/UIActionBindingHandle.h>

#include "PauseMenuWidget.generated.h"

class UInputAction;
class UOverlay;
class UTextBlock;

namespace ml::ioj {
class UMenuButtonWidget;

DECLARE_MULTICAST_DELEGATE(FPauseReturnToLevelSelectRequested);
DECLARE_MULTICAST_DELEGATE(FPauseQuitRequested);

enum class EPauseMenuTab : uint8 {
    Overview,
    Stats,
    Options,
};

UCLASS()
class SPACEGAME_API UPauseMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto get_active_tab() const noexcept -> EPauseMenuTab { return active_tab; }
    void prepare_for_open(UInputAction& toggle_action);

    FPauseReturnToLevelSelectRequested return_to_level_select_requested;
    FPauseQuitRequested quit_requested;
  protected:
    void NativeOnInitialized() override;
    void NativeOnActivated() override;
    void NativeOnDeactivated() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* resume_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* overview_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* stats_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* options_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* return_to_level_select_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* quit_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* page_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* page_placeholder{nullptr};
  private:
    void handle_resume();
    void handle_overview();
    void handle_stats();
    void handle_options();
    void handle_return_to_level_select();
    void handle_quit();
    void handle_toggle_action();

    void set_active_tab(EPauseMenuTab tab);

    EPauseMenuTab active_tab{EPauseMenuTab::Overview};
    TWeakObjectPtr<UInputAction> toggle_action_;
    FUIActionBindingHandle toggle_action_binding_;
    bool terminal_action_requested_{false};
};
}
