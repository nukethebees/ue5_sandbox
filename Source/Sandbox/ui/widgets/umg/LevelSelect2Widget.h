#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/GridPanel.h"

#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/widgets/CommonMenuDelegates.h"
#include "Sandbox/widgets/LoadLevelButtonWidget.h"
#include "Sandbox/widgets/TextButtonWidget.h"

#include "LevelSelect2Widget.generated.h"

UCLASS()
class SANDBOX_API ULevelSelect2Widget
    : public UUserWidget
    , public ml::LogMsgMixin<"ULevelSelect2Widget"> {
    GENERATED_BODY()
  public:
    UPROPERTY(BlueprintAssignable)
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
