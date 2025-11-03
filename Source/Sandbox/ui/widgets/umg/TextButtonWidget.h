#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "TextButtonWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnTextButtonClicked);

UCLASS()
class SANDBOX_API UTextButtonWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION()
    void set_label(FText const& txt);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ExposeOnSpawn = true))
    FText label{};

    FOnTextButtonClicked on_clicked;
  protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void SynchronizeProperties() override;

    UPROPERTY(meta = (BindWidget))
    UButton* button_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* text_block{nullptr};
  private:
    UFUNCTION()
    void handle_click();
    void set_label();
};
