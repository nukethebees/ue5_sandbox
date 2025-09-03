#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "LoadLevelButtonWidget.generated.h"

class UButton;
class UTextBlock;

enum class DisplayNameChanged : uint8 {
    yes, no
};

UCLASS()
class SANDBOX_API ULoadLevelButtonWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_level_path(FName value);
    void set_level_display_name(FText value);
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* load_level_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* level_text_block{nullptr};

    UPROPERTY(BlueprintReadOnly)
    FText level_display_name_{};
    UPROPERTY()
    FName level_path_{};
  private:
    void load_level();
    DisplayNameChanged set_level_display_name_to_path_if_unset();
    void update_display_name_text_box();
};
