#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGame/ui/style/GameUiStyle.h"

#include "MainMenuWidget.generated.h"

namespace ml::ioj {
enum class EOptionsTab : uint8;
class SMainMenuView;
class UDebugSettingsWidget;
class UGameSubsystem;
class ULevelSelectWidget;
class UOptionsWidget;
class USaveGameViewerWidget;

enum class EMainMenuPage : uint8 {
    SelectMission,
    DataArchive,
    Video,
    Gameplay,
    Audio,
    Controls,
    Accessibility,
    System,
    Debug,
};

UCLASS()
class SPACEGAME_API UMainMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    UMainMenuWidget();

    void prepare_for_open(TSubclassOf<ULevelSelectWidget> level_select_class,
                          bool focus_mission_content,
                          FName preferred_level_id);
    void select_page(EMainMenuPage page);

    [[nodiscard]] auto get_active_page() const noexcept -> EMainMenuPage { return active_page_; }
    [[nodiscard]] auto get_level_select_widget() const -> ULevelSelectWidget* {
        return level_select_widget_;
    }
    [[nodiscard]] auto get_options_widget() const -> UOptionsWidget* { return options_widget_; }
    [[nodiscard]] auto get_debug_settings_widget() const -> UDebugSettingsWidget* {
        return debug_settings_widget_;
    }
    [[nodiscard]] auto get_save_game_viewer() const -> USaveGameViewerWidget* {
        return save_game_viewer_;
    }
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto NativeOnHandleBackAction() -> bool override;
  private:
    static auto is_options_page(EMainMenuPage page) -> bool;
    static auto options_tab_for_page(EMainMenuPage page) -> EOptionsTab;

    void create_content_widgets();
    void request_page(EMainMenuPage page);
    void show_page(EMainMenuPage page);
    void request_quit();
    void quit_game();
    void focus_active_content();
    void handle_page_modal_changed(bool visible);
    void handle_profile_debug_settings_changed();

    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    UPROPERTY(Transient)
    ULevelSelectWidget* level_select_widget_{nullptr};
    UPROPERTY(Transient)
    USaveGameViewerWidget* save_game_viewer_{nullptr};
    UPROPERTY(Transient)
    UOptionsWidget* options_widget_{nullptr};
    UPROPERTY(Transient)
    UDebugSettingsWidget* debug_settings_widget_{nullptr};

    TSubclassOf<ULevelSelectWidget> level_select_class_{};
    TSharedPtr<SMainMenuView> view_{};
    FGameUiStyle fallback_style_{};
    EMainMenuPage active_page_{EMainMenuPage::SelectMission};
    FName preferred_level_id_{NAME_None};
    bool focus_mission_content_{};
    bool options_open_{};
    bool page_modal_visible_{};
};
} // namespace ml::ioj
