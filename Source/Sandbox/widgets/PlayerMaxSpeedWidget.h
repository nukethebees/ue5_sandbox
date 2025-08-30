#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"

#include "PlayerMaxSpeedWidget.generated.h"

UCLASS()
class SANDBOX_API UPlayerMaxSpeedWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable)
    void update(float speed);
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* text;
};
