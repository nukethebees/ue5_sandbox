#pragma once

#include <ioj/input/dispatch/player_menu.h>

#include "Blueprint/UserWidget.h"

#include "OptionsWidget.generated.h"

namespace ioj {
struct MenuUpdate;
enum class GameSettingCategory : uint8;
class SGameOptionsView;
class UGameUiStyleSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FOptionsModalStateChanged, bool);

UCLASS()
class SPACEGAME_API UOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UOptionsWidget(FObjectInitializer const& object_initializer);

    auto page() const -> MainMenuItem;
    void show_page(MainMenuItem page);
    void refresh_view();
    void focus_content();
    void present(MenuUpdate const& update);

    FOptionsModalStateChanged modal_state_changed;
    FSimpleMulticastDelegate content_focused;
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    void NativeOnAddedToFocusPath(FFocusEvent const& event) override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void handle_interaction_modal_changed(bool visible);

    UPROPERTY(Transient)
    UGameUiStyleSubsystem* style_{nullptr};

    TSharedPtr<SGameOptionsView> options_view_{};
    TOptional<MainMenuItem> rendered_page_{};
};
} // namespace ioj
