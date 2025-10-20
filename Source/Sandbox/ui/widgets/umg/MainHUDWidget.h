#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/slate_widgets/NumWidget.h"
#include "Sandbox/widgets/HealthWidget.h"
#include "Sandbox/widgets/ValueWidget.h"

#include "MainHUDWidget.generated.h"

UCLASS()
class SANDBOX_API UMainHUDWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UMainHUDWidget"> {
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

    UIntNumWidget* ammo_widget{nullptr};
  protected:
    virtual void NativeConstruct() override;
  public:
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_health(FHealthData health_data);
  private:
    void missing_widget_error(FStringView method);
};
