#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "MainHUDWidget.generated.h"

class UVerticalBox;
class UValueWidget;
class UHealthWidget;
class UIntNumWidget;
class UItemDescriptionHUDWidget;

UCLASS()
class SANDBOX_API UMainHUDWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UMainHUDWidget", LogSandboxUI> {
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

    UPROPERTY()
    UIntNumWidget* ammo_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UItemDescriptionHUDWidget* item_description_widget{nullptr};
  protected:
    virtual void NativeConstruct() override;
  public:
    UFUNCTION(BlueprintCallable, Category = "UI")
    void update_health(FHealthData health_data);
};
