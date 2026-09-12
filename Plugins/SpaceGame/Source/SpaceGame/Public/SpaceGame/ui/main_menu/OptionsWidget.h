#pragma once

#include "Blueprint/UserWidget.h"
#include "OptionsWidget.generated.h"

namespace ml::ioj {
enum class EGameSettingCategory : uint8;
class SGameOptionsView;
class UGameSettingsSubsystem;
class UGameSubsystem;

enum class EOptionsTab : uint8 {
    Video,
    Gameplay,
    Audio,
    Controls,
    Accessibility,
    System,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOptionsModalStateChanged, bool);

UCLASS()
class SPACEGAME_API UOptionsWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UOptionsWidget(FObjectInitializer const& object_initializer);

    [[nodiscard]] auto get_active_tab() const noexcept -> EOptionsTab { return active_tab_; }
    void select_tab(EOptionsTab tab);
    void prepare_for_open();
    void request_leave(FSimpleDelegate continuation);
    void request_back();
    void focus_content();
    [[nodiscard]] auto get_focus_target() const -> UWidget*;

    FOptionsModalStateChanged modal_state_changed;
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void handle_tab_changed(EOptionsTab tab);
    void handle_apply();
    void handle_reset();
    void handle_dirty_apply();
    void handle_dirty_discard();
    void handle_dirty_stay();
    void handle_confirm_display();
    void handle_revert_display();
    void handle_display_confirmation_changed(bool visible);
    void handle_interaction_modal_changed(bool visible);
    void refresh_view();
    auto active_category() const -> TOptional<EGameSettingCategory>;
    void complete_leave();
    void cancel_leave();

    UPROPERTY(Transient)
    UGameSettingsSubsystem* settings_{nullptr};

    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};

    TSharedPtr<SGameOptionsView> options_view_{};
    FSimpleDelegate leave_continuation_{};
    EOptionsTab active_tab_{EOptionsTab::Video};
    bool exit_after_confirmation_{};
};
} // namespace ml::ioj
