#pragma once

#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGameS7/LevelScriptCatalog.h>

#include <Components/Button.h>

#include "ScriptLevelSelectWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UMultiLineEditableTextBox;

namespace ml::ioj {
enum class ELevelLaunchMode : uint8;
}

namespace ml::s7 {
DECLARE_MULTICAST_DELEGATE_OneParam(FLevelScriptSelected, int32);

UCLASS()
class SPACEGAMES7_API ULevelScriptButton : public UButton {
    GENERATED_BODY()
  public:
    void initialise(int32 index, FString const& label, bool valid);
    void set_selected(bool selected);

    FLevelScriptSelected selected;
  private:
    UFUNCTION()
    void handle_clicked();

    int32 index_{INDEX_NONE};
    bool valid_{false};
};

UCLASS()
class SPACEGAMES7_API UScriptLevelSelectWidget : public ml::ioj::ULevelSelectWidget {
    GENERATED_BODY()
  public:
    void activate() override;
  protected:
    void NativeOnInitialized() override;
  private:
    auto create_script_preview() -> bool;
    void launch_selected_level(ml::ioj::ELevelLaunchMode launch_mode);
    void refresh_levels();
    void select_level(int32 index);

    UFUNCTION()
    void handle_refresh();
    UFUNCTION()
    void handle_launch();
    UFUNCTION()
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
    UButton* refresh_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* launch_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* start_paused_button{nullptr};

    TArray<FLevelScriptEntry> entries_{};
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULevelScriptButton>> level_buttons_{};
    UPROPERTY(Transient)
    TObjectPtr<UMultiLineEditableTextBox> script_preview_{nullptr};
    int32 selected_index_{INDEX_NONE};
};
}
