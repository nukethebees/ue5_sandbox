#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/GridPanel.h"

#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/ui/CommonMenuDelegates.h"
#include "Sandbox/ui/main_menu/LoadLevelButtonWidget.h"
#include "Sandbox/ui/widgets/TextButtonWidget.h"

#include "LevelSelect2Widget.generated.h"

UCLASS()
class SANDBOX_API ULevelSelect2Widget
    : public UUserWidget
    , public ml::LogMsgMixin<"ULevelSelect2Widget"> {
    GENERATED_BODY()
  public:
    FBackRequested back_requested;

    void populate_level_buttons(TArray<FName> const& level_names);
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UGridPanel* level_select_grid;
    UPROPERTY(meta = (BindWidget))
    UTextButtonWidget* back_button;

    UPROPERTY(EditDefaultsOnly, Category = "Level Select")
    TSubclassOf<ULoadLevelButtonWidget> load_level_button_class;
  private:
    UFUNCTION()
    void handle_back();
};
