#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Sandbox/widgets/FuelWidget.h"
#include "Sandbox/widgets/JumpWidget.h"
#include "Sandbox/widgets/CoinWidget.h"

#include "MainHUDWidget.generated.h"

UCLASS()
class SANDBOX_API UMainHUDWidget : public UUserWidget {
    GENERATED_BODY()
  public:
  protected:
    // Widget instances
    UPROPERTY(meta = (BindWidget))
    UFuelWidget* fuel_widget;

    UPROPERTY(meta = (BindWidget))
    UJumpWidget* jump_widget;

    UPROPERTY(meta = (BindWidget))
    UCoinWidget* coin_widget;
  public:
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_fuel(float new_fuel);
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_jump(int32 new_jump);
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_coin(int32 new_coin_count);
  private:
    void missing_widget_error(FStringView method);
};
