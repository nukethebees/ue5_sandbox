#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipPointsWidget.generated.h"

class UValueWidget;

UCLASS()
class SPACEGAME_API UShipPointsWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_points(int32 points);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32;
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
