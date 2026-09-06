#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShipSpeedWidget.generated.h"

class UValueWidget;
struct FTextBlockStyle;
namespace ml::ioj {
struct FGameHudStyle;
}

UCLASS()
class SPACEGAME_API UShipSpeedWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_speed(float speed);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
    void set_font_size(int32 const new_font_size);
    void set_text_style(FTextBlockStyle const& style);
    auto get_font_size() const noexcept -> int32;
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
