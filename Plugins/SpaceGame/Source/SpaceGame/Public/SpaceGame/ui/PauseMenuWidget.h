#pragma once

#include "Blueprint/UserWidget.h"

#include "PauseMenuWidget.generated.h"

class UButton;
class UOverlay;
class UTextBlock;

namespace ml::ioj {
enum class EPauseMenuTab : uint8 {
    Overview,
    Stats,
    Options,
};

DECLARE_MULTICAST_DELEGATE(FPauseMenuResumeRequested);

UCLASS()
class SPACEGAME_API UPauseMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto get_active_tab() const noexcept -> EPauseMenuTab { return active_tab; }

    void focus_resume_button();

    FPauseMenuResumeRequested resume_requested;
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* resume_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* overview_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* stats_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* options_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* page_heading{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* page_placeholder{nullptr};
  private:
    UFUNCTION()
    void handle_resume();
    UFUNCTION()
    void handle_overview();
    UFUNCTION()
    void handle_stats();
    UFUNCTION()
    void handle_options();

    void set_active_tab(EPauseMenuTab tab);
    void set_tab_button_state(UButton& button, bool selected);

    EPauseMenuTab active_tab{EPauseMenuTab::Overview};
};
}
