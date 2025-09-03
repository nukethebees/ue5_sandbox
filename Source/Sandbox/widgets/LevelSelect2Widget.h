#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/GridPanel.h"
#include "CoreMinimal.h"
#include "Sandbox/widgets/LoadLevelButtonWidget.h"
#include "Sandbox/widgets/TextButtonWidget.h"

#include "LevelSelect2Widget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackRequested);

UCLASS()
class SANDBOX_API ULevelSelect2Widget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(BlueprintAssignable)
    FOnBackRequested back_requested;

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
