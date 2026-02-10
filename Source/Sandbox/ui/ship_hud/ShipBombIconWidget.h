#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipBombIconWidget.generated.h"

class UMaterialInterface;
class UImage;

UCLASS()
class SANDBOX_API UShipBombIconWidget : public UUserWidget {
  public:
    GENERATED_BODY()
  protected:
    void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UImage* bomb_image;
    UPROPERTY(EditAnywhere, Category = "Ship")
    UMaterialInterface* bomb_material{nullptr};
};
