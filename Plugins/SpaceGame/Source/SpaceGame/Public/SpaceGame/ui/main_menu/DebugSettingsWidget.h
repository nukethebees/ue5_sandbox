#pragma once

#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "Blueprint/UserWidget.h"

#include "DebugSettingsWidget.generated.h"

class USpaceSaveSubsystem;

namespace ml::ioj {
class SDebugSettingsView;
class UGameSubsystem;

DECLARE_MULTICAST_DELEGATE(FProfileDebugSettingsChanged);

UCLASS()
class SPACEGAME_API UDebugSettingsWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UDebugSettingsWidget(FObjectInitializer const& object_initializer);

    void refresh();
    void focus_content();

    FProfileDebugSettingsChanged settings_changed;
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    [[nodiscard]] auto setting_available() const -> bool;
    [[nodiscard]] auto unlock_all_missions() const -> bool;
    [[nodiscard]] auto start_levels_paused() const -> bool;
    [[nodiscard]] auto status_text() const -> FText;
    void handle_unlock_all_missions_changed(ECheckBoxState state);
    void handle_start_levels_paused_changed(ECheckBoxState state);

    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    UPROPERTY(Transient)
    USpaceSaveSubsystem* save_{nullptr};

    TSharedPtr<SDebugSettingsView> view_{};
    FGameUiStyle fallback_style_{};
    bool save_failed_{};
};
} // namespace ml::ioj
