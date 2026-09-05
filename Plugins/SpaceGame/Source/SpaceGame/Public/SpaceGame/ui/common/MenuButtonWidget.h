#pragma once

#include "SpaceGame/ui/style/GameUiStyle.h"

#include <CommonButtonBase.h>
#include <CommonTextBlock.h>

#include "MenuButtonWidget.generated.h"

class UBorder;

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
    void NativeOnPressed() override;
    void NativeOnReleased() override;
    void NativeOnCurrentTextStyleChanged() override;

    UPROPERTY(EditAnywhere, Category = "Menu Button")
    FText text_{};

    UPROPERTY(EditAnywhere, Category = "Menu Button|Style")
    EGameButtonStyle style_role_{EGameButtonStyle::Primary};

    UPROPERTY(EditAnywhere, Category = "Menu Button|Style", meta = (InlineEditConditionToggle))
    bool has_style_override_{false};

    UPROPERTY(EditAnywhere,
              Category = "Menu Button|Style",
              meta = (EditCondition = "has_style_override_"))
    FGameButtonPresentationStyle style_override_{};

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UBorder* background{nullptr};

    UPROPERTY(meta = (BindWidget))
    UCommonTextBlock* label_text{nullptr};
  private:
    auto resolve_style() const -> FGameButtonPresentationStyle;
    void update_visual_style();

    FGameButtonPresentationStyle resolved_style_{};
};
}
