#pragma once

#include "Blueprint/UserWidget.h"
#include "SandboxGameShared/ui/CommonMenuDelegates.h"

#include "OptionsWidget.generated.h"

class UButton;
class UOverlay;
class UWidget;
class UWidgetSwitcher;

namespace ml::ioj {
enum class EOptionsTab : uint8 {
    Video,
    Gameplay,
    Audio,
    Controls,
    Accessibility,
};

UCLASS()
class SPACEGAME_API UOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto get_active_tab() const noexcept -> EOptionsTab { return active_tab_; }

    void focus_active_tab();
    [[nodiscard]] auto get_focus_target() const -> UWidget*;

    FBackRequested back_requested;
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* tab_switcher{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* video_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* gameplay_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* audio_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* controls_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* accessibility_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* back_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UWidget* video_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    UWidget* gameplay_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    UWidget* audio_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    UWidget* controls_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    UWidget* accessibility_page{nullptr};
  private:
    UFUNCTION()
    void handle_video();
    UFUNCTION()
    void handle_gameplay();
    UFUNCTION()
    void handle_audio();
    UFUNCTION()
    void handle_controls();
    UFUNCTION()
    void handle_accessibility();
    UFUNCTION()
    void handle_back();

    void set_active_tab(EOptionsTab tab);
    void set_tab_button_state(UButton& button, bool selected);

    EOptionsTab active_tab_{EOptionsTab::Video};
};
}
