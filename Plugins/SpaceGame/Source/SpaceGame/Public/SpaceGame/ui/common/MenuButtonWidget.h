#pragma once

#include <CommonButtonBase.h>
#include <CommonTextBlock.h>

#include "MenuButtonWidget.generated.h"

namespace ml::ioj {
UCLASS()
class SPACEGAME_API UMenuTextStyle : public UCommonTextStyle {
    GENERATED_BODY()
  public:
    UMenuTextStyle();
};

UCLASS()
class SPACEGAME_API UMenuButtonStyle : public UCommonButtonStyle {
    GENERATED_BODY()
  public:
    UMenuButtonStyle();
};

UCLASS()
class SPACEGAME_API UMenuButtonWidget : public UCommonButtonBase {
    GENERATED_BODY()
  public:
    UMenuButtonWidget();

    void set_text(FText const& text);
    [[nodiscard]] auto get_text() const -> FText;
  protected:
    void NativePreConstruct() override;
    void NativeOnCurrentTextStyleChanged() override;

    UPROPERTY(EditAnywhere, Category = "Menu Button")
    FText text_{};

    UPROPERTY(meta = (BindWidget))
    UCommonTextBlock* label_text{nullptr};
};
}
