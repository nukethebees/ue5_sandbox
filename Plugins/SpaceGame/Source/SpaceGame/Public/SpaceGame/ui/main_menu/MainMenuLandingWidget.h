#pragma once

#include "Blueprint/UserWidget.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include "MainMenuLandingWidget.generated.h"

namespace ml::ioj {
class SMainMenuView;
class UGameSubsystem;

enum class EMainMenuAction : uint8 {
    SelectMission,
    SaveData,
    Options,
    QuitGame,
};

DECLARE_MULTICAST_DELEGATE(FMainMenuActionRequested);

UCLASS()
class SPACEGAME_API UMainMenuLandingWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UMainMenuLandingWidget(FObjectInitializer const& object_initializer);

    void set_preferred_action(EMainMenuAction action);
    void focus_preferred_action();

    FMainMenuActionRequested select_mission_requested;
    FMainMenuActionRequested save_data_requested;
    FMainMenuActionRequested options_requested;
    FMainMenuActionRequested quit_game_requested;
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void handle_select_mission();
    void handle_save_data();
    void handle_options();
    void handle_quit_game();

    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};

    TSharedPtr<SMainMenuView> view_{};
    FGameUiStyle fallback_style_{};
    EMainMenuAction preferred_action_{EMainMenuAction::SelectMission};
};
} // namespace ml::ioj
