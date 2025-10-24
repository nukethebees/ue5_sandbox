#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ItemDescriptionHUDWidget.generated.h"

UCLASS()
class SANDBOX_API UItemDescriptionHUDWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
  public:
    UFUNCTION(BlueprintCallable, Category = "UI")
    void set_description(FText const& description);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void clear_description();
  protected:
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* description_text{nullptr};
};
