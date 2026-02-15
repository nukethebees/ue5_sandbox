#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipRenderIconWidget.generated.h"

class UMaterialInterface;
class UImage;

UCLASS()
class SANDBOX_API UShipRenderIconWidget : public UUserWidget {
  public:
    GENERATED_BODY()
  protected:
    void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UImage* item_image;
    UPROPERTY(EditAnywhere, Category = "Ship")
    UMaterialInterface* item_material{nullptr};
};
