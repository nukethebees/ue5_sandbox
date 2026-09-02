#pragma once

#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGameS7/LevelScriptCatalog.h>

#include <Components/Button.h>

#include "ScriptLevelSelectWidget.generated.h"

class UTextBlock;
class UVerticalBox;

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
    void refresh_levels();
    void select_level(int32 index);

    UFUNCTION()
    void handle_refresh();
    UFUNCTION()
    void handle_launch();

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
    UButton* refresh_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* launch_button{nullptr};

    TArray<FLevelScriptEntry> entries_{};
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULevelScriptButton>> level_buttons_{};
    int32 selected_index_{INDEX_NONE};
};
}
