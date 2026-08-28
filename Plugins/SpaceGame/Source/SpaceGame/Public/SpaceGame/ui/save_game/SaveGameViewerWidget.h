#pragma once

#include "Blueprint/UserWidget.h"

#include "SaveGameViewerWidget.generated.h"

class UButton;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;

namespace ml::ioj {
struct FSaveGameBrowser;
struct FSaveGameSummary;
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
    UVerticalBox* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UScrollBox* save_list{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* empty_state_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* detail_switcher{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* empty_detail_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* selected_detail_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* detail_name_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* detail_id_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* detail_scenario_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* detail_date_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* detail_duration_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* detail_score_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* detail_result_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* refresh_button{nullptr};
  private:
    UFUNCTION()
    void handle_refresh();

    auto resolve_browser() -> FSaveGameBrowser*;
    void rebuild_list();
    void select_save(FString const& save_id);
    void show_summary(FSaveGameSummary const& summary);
    void show_empty_state();

    UPROPERTY(Transient)
    TArray<TObjectPtr<USaveGameRowWidget>> rows_{};

    UPROPERTY()
    TSubclassOf<USaveGameRowWidget> row_widget_class_{nullptr};

    FString selected_save_id_{};
    FSaveGameBrowser* browser_override_{nullptr};
};
}
