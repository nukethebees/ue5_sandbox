#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"

#include "CoinWidget.generated.h"

UCLASS()
class SANDBOX_API UCoinWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable)
    void update_coin(int32 coins);
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* coin_text;
};
