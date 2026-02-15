#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipGoldRingCountWidget.generated.h"

class UUniformGridPanel;
class UUniformGridSlot;
class UMaterialInterface;

class UShipRenderIconWidget;

UCLASS()
class SANDBOX_API UShipGoldRingCountWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_count(int32 const count);
  protected:
    static void set_slot_alignment(UUniformGridSlot* slot);

    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* grid{nullptr};
    UPROPERTY(EditAnywhere, Category = "Ship")
    TSubclassOf<UShipRenderIconWidget> icon_widget_class;
};
