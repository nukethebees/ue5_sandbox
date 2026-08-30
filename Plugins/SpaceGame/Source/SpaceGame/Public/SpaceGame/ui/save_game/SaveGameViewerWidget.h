#pragma once

#include "Blueprint/UserWidget.h"

#include "SaveGameViewerWidget.generated.h"

class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class USpaceSaveSubsystem;

namespace ml::ioj {
struct FLevelOutcomeSummary;
struct FSaveGameBrowser;
struct FSaveProfileReport;
struct FSaveProfileSummary;
class ULevelOutcomeRowWidget;
class USaveGameRowWidget;

UCLASS()
class SPACEGAME_API USaveGameViewerWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    USaveGameViewerWidget(FObjectInitializer const& object_initializer);

    void set_browser(FSaveGameBrowser& browser);
    void focus_primary_action();
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UHorizontalBox* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UScrollBox* profile_list{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_empty_state_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* refresh_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* create_profile_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* create_profile_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UEditableTextBox* profile_name_input{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_create_error_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* confirm_create_profile_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* cancel_create_profile_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UScrollBox* outcome_list{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* outcome_empty_state_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* report_switcher{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* empty_report_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* selected_report_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_name_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_id_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_created_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_last_played_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_duration_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* profile_score_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* active_profile_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* activate_profile_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UScrollBox* results_scroll_box{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* result_sections_box{nullptr};
  private:
    UFUNCTION()
    void handle_refresh();
    UFUNCTION()
    void handle_begin_create_profile();
    UFUNCTION()
    void handle_confirm_create_profile();
    UFUNCTION()
    void handle_cancel_create_profile();
    UFUNCTION()
    void handle_activate_profile();

    auto resolve_browser() -> FSaveGameBrowser*;
    auto resolve_save_subsystem() const -> USpaceSaveSubsystem*;
    void refresh_and_select(FString const& profile_id);
    void show_create_profile_error(FText const& error);
    void rebuild_profiles();
    void select_profile(FString const& profile_id);
    void rebuild_outcomes(FSaveProfileReport const& report);
    void select_outcome(FString const& outcome_id);
    void show_profile(FSaveProfileSummary const& profile);
    void add_result_section(FLevelOutcomeSummary const& outcome, int32 index);
    void show_empty_profiles();

    UPROPERTY(Transient)
    TArray<TObjectPtr<USaveGameRowWidget>> profile_rows_{};
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULevelOutcomeRowWidget>> outcome_rows_{};
    UPROPERTY(Transient)
    TArray<TObjectPtr<UWidget>> result_sections_{};

    UPROPERTY()
    TSubclassOf<USaveGameRowWidget> profile_row_widget_class_{nullptr};
    UPROPERTY()
    TSubclassOf<ULevelOutcomeRowWidget> outcome_row_widget_class_{nullptr};

    FString selected_profile_id_{};
    FString selected_outcome_id_{};
    FSaveGameBrowser* browser_override_{nullptr};
};
}
