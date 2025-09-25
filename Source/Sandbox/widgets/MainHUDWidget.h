#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Sandbox/widgets/HealthWidget.h"
#include "Sandbox/widgets/ValueWidget.h"
#include "Components/VerticalBox.h"

#include "MainHUDWidget.generated.h"

UCLASS()
class SANDBOX_API UMainHUDWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* current_stat_box{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* fuel_widget;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* jump_widget;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* coin_widget;

    UPROPERTY(meta = (BindWidget))
    UHealthWidget* health_widget;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* max_speed_widget;
  public:
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_health(FHealthData health_data);
  private:
    void missing_widget_error(FStringView method);
};
