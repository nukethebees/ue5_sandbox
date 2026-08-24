#pragma once

#include "SpaceGame/ships/common/ShipHealth.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipHealthWidget.generated.h"

class UProgressBar;

class UValueWidget;

UCLASS()
class SPACEGAME_API UShipHealthWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_health(FShipHealth health);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32;
  protected:
    UPROPERTY(meta = (BindWidget))
    UProgressBar* health_bar{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* health_text{nullptr};
};
