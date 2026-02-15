#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipBombCountWidget.generated.h"

class UUniformGridPanel;
class UMaterialInterface;

class UShipRenderIconWidget;

UCLASS()
class SANDBOX_API UShipBombCountWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_count(int32 count);
  protected:
    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* grid{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ship")
    TSubclassOf<UShipRenderIconWidget> icon_widget_class;
};
