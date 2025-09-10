#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Sandbox/widgets/HealthWidget.h"
#include "Sandbox/widgets/JumpWidget.h"
#include "Sandbox/widgets/ValueWidget.h"

#include "MainHUDWidget.generated.h"

UCLASS()
class SANDBOX_API UMainHUDWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    // Widget instances
    UPROPERTY(meta = (BindWidget))
    UValueWidget* fuel_widget;

    UPROPERTY(meta = (BindWidget))
    UJumpWidget* jump_widget;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* coin_widget;

    UPROPERTY(meta = (BindWidget))
    UHealthWidget* health_widget;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* max_speed_widget;
  public:
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_jump(int32 new_jump);
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_health(FHealthData health_data);
  private:
    void missing_widget_error(FStringView method);
};
