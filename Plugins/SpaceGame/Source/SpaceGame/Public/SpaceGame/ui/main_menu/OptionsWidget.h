#pragma once

#include "Blueprint/UserWidget.h"
#include "SandboxGameShared/ui/CommonMenuDelegates.h"

#include "OptionsWidget.generated.h"

class UButton;
class UBorder;
class UOverlay;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;

namespace ml::ioj {
enum class EGameSettingCategory : uint8;
class UGameSettingsSubsystem;
class USettingsRowWidget;

enum class EOptionsTab : uint8 {
    Video,
    Gameplay,
    Audio,
    Controls,
    Accessibility,
    System,
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
    void handle_system();
    UFUNCTION()
    void handle_back();
    UFUNCTION()
    void handle_apply();
    UFUNCTION()
    void handle_reset();
    UFUNCTION()
    void handle_dirty_apply();
    UFUNCTION()
    void handle_dirty_discard();
    UFUNCTION()
    void handle_dirty_stay();
    UFUNCTION()
    void handle_confirm_display();
    UFUNCTION()
    void handle_revert_display();

    void set_active_tab(EOptionsTab tab);
    void set_tab_button_state(UButton& button, bool selected);
    void build_system_tab();
    void build_settings_pages();
    void build_category_page(UWidget& page, EGameSettingCategory category);
    void build_system_page();
    void build_modals();
    void refresh_settings_ui();
    void handle_display_confirmation_changed(bool visible);
    auto active_category() const -> EGameSettingCategory;

    UPROPERTY(Transient)
    UGameSettingsSubsystem* settings_{nullptr};

    UPROPERTY(Transient)
    TArray<TObjectPtr<USettingsRowWidget>> settings_rows_;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> apply_buttons_;

    UPROPERTY(Transient)
    UBorder* dirty_modal_{nullptr};

    UPROPERTY(Transient)
    UBorder* display_modal_{nullptr};

    UPROPERTY(Transient)
    UTextBlock* display_countdown_{nullptr};

    UPROPERTY(Transient)
    UButton* system_button_{nullptr};

    UPROPERTY(Transient)
    UVerticalBox* system_page_{nullptr};

    EOptionsTab active_tab_{EOptionsTab::Video};
    bool settings_pages_built_{};
    bool system_page_built_{};
};
}
