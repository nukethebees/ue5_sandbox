#pragma once

#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Blueprint/UserWidget.h>

#include "MenuButtonWidget.generated.h"

class UBorder;

class UButton;
class USizeBox;
class UTextBlock;

namespace ioj {
UCLASS()
class SPACEGAMEPRESENTATION_API UMenuButtonWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    FSimpleMulticastDelegate clicked;
    auto focus_target() const -> UWidget*;

    void set_text(FText const& text);
    [[nodiscard]] auto get_text() const -> FText;
    void set_audio(FGameAudioFacade audio);
  protected:
    void NativePreConstruct() override;
    void NativeOnInitialized() override;
    void NativeOnAddedToFocusPath(FFocusEvent const& focus_event) override;
    void NativeOnRemovedFromFocusPath(FFocusEvent const& focus_event) override;

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
    USizeBox* button_size{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UBorder* background{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* label_text{nullptr};
  private:
    auto resolve_style() const -> FGameButtonPresentationStyle;
    UFUNCTION()
    void update_visual_style();
    UFUNCTION()
    void handle_click();
    UFUNCTION()
    void handle_press();

    FGameButtonPresentationStyle resolved_style_{};
    FGameAudioFacade audio_{};
};
}
