#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TextButtonWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTextButtonClicked);

UCLASS()
class SANDBOX_API UTextButtonWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ExposeOnSpawn = true))
    FText label{};

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTextButtonClicked on_clicked;
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* text_block{nullptr};
  private:
    void handle_click();
};
