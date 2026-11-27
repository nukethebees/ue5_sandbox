#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "MainMenuWidget.generated.h"

namespace ioj {
enum class EMenuPage : uint8;
class SMainMenuView;
class UGameUiStyleSubsystem;
class ULevelSelectWidget;
class UOptionsWidget;
class USaveGameViewerWidget;

UCLASS()
class SPACEGAME_API UMainMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    UMainMenuWidget();

    void prepare_for_open(TSubclassOf<ULevelSelectWidget> level_select_class,
                          bool focus_level_content,
                          FName preferred_level_id);
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto NativeOnHandleBackAction() -> bool override;
  private:
    void create_content_widgets();
    void request_page(EMenuPage page);
    void show_page(EMenuPage page);
    void request_quit();
    void quit_game();
    void focus_active_content();
    void handle_page_modal_changed(bool visible);

    UPROPERTY(Transient)
    UGameUiStyleSubsystem* style_{nullptr};
    UPROPERTY(Transient)
    ULevelSelectWidget* level_select_widget_{nullptr};
    UPROPERTY(Transient)
    USaveGameViewerWidget* save_game_viewer_{nullptr};
    UPROPERTY(Transient)
    UOptionsWidget* options_widget_{nullptr};

    TSubclassOf<ULevelSelectWidget> level_select_class_{};
    TSharedPtr<SMainMenuView> view_{};
    FGameUiStyle fallback_style_{};
    FName preferred_level_id_{NAME_None};
    bool focus_level_content_{};
    bool page_modal_visible_{};
};
} // namespace ioj
