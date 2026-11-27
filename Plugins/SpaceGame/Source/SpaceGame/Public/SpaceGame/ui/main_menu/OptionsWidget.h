#pragma once

#include "SpaceGame/ui/main_menu/MenuPage.h"

#include "Blueprint/UserWidget.h"

#include "OptionsWidget.generated.h"

namespace ioj {
enum class GameSettingCategory : uint8;
class SGameOptionsView;
class UOptionsDraft;
class UGameUiStyleSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FOptionsModalStateChanged, bool);

UCLASS()
class SPACEGAME_API UOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UOptionsWidget(FObjectInitializer const& object_initializer);

    auto page() const -> EMenuPage { return page_; }
    void select_page(EMenuPage page, FSimpleDelegate shown);
    void request_leave(FSimpleDelegate continuation, FSimpleDelegate cancelled = {});
    void request_back();
    void focus_content();

    FOptionsModalStateChanged modal_state_changed;
    FSimpleMulticastDelegate content_focused;
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    void NativeOnAddedToFocusPath(FFocusEvent const& event) override;
    void NativeDestruct() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void open_page(EMenuPage page, FSimpleDelegate shown);
    void handle_apply();
    void handle_reset();
    void handle_dirty_apply();
    void handle_save_completed(bool succeeded);
    void handle_dirty_discard();
    void handle_dirty_stay();
    void handle_confirm_display();
    void handle_revert_display();
    void handle_display_confirmation_changed(bool visible);
    void handle_interaction_modal_changed(bool visible);
    void refresh_view();
    void complete_leave();
    void cancel_leave();

    UPROPERTY(Transient)
    UOptionsDraft* settings_{nullptr};

    UPROPERTY(Transient)
    UGameUiStyleSubsystem* style_{nullptr};

    TSharedPtr<SGameOptionsView> options_view_{};
    FSimpleDelegate leave_continuation_{};
    FSimpleDelegate leave_cancelled_{};
    EMenuPage page_{EMenuPage::SelectLevel};
};
} // namespace ioj
