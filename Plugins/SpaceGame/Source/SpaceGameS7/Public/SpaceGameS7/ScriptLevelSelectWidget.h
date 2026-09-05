#pragma once

#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGameS7/LevelScriptCatalog.h>

#include "ScriptLevelSelectWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UMultiLineEditableTextBox;

namespace ml::ioj {
enum class ELevelLaunchMode : uint8;
}

namespace ml::s7 {
enum class ELevelRowState : uint8 {
    Invalid,
    Locked,
    Unlocked,
    Completed,
};

SPACEGAMES7_API auto format_level_row_title(FString title, ELevelRowState state) -> FString;

UCLASS()
class SPACEGAMES7_API UScriptLevelSelectWidget : public ml::ioj::ULevelSelectWidget {
    GENERATED_BODY()
  public:
    UScriptLevelSelectWidget();
  protected:
    void NativeOnInitialized() override;
    void NativeOnActivated() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
  private:
    auto create_script_preview() -> bool;
    void launch_selected_level(ml::ioj::ELevelLaunchMode launch_mode);
    void refresh_levels();
    void select_level(int32 button_index);
    void restore_level_selection(int32 button_index);
    void apply_level_selection(int32 button_index, bool refresh_focus);

    void handle_refresh();
    void handle_launch();
    void handle_start_paused();
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* level_list{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* selected_file_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* title_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* description_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* status_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* details_text{nullptr};
    UPROPERTY(meta = (BindWidget))
    ml::ioj::UMenuButtonWidget* refresh_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    ml::ioj::UMenuButtonWidget* launch_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    ml::ioj::UMenuButtonWidget* start_paused_button{nullptr};

    TArray<FLevelScriptEntry> entries_{};
    UPROPERTY(Transient)
    TArray<TObjectPtr<ml::ioj::UMenuButtonWidget>> level_buttons_{};
    TArray<int32> level_entry_indices_{};
    UPROPERTY(Transient)
    TObjectPtr<UMultiLineEditableTextBox> script_preview_{nullptr};
    UPROPERTY()
    TSubclassOf<ml::ioj::UMenuButtonWidget> menu_button_class_{nullptr};
    UPROPERTY(Transient)
    TObjectPtr<UWidget> desired_focus_target_{nullptr};
    int32 selected_entry_index_{INDEX_NONE};
    FName selected_level_id_{NAME_None};
};
}
