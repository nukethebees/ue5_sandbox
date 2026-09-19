#pragma once

#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Vector2DWidget.generated.h"

class UBorder;
class UCanvasPanel;
class USizeBox;
class UTextBlock;
UCLASS()
class SPACEGAMEPRESENTATION_API UVector2DWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void update(FVector2D const value);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
    void set_font_size(int32 const new_font_size);
    void set_label(FText new_label);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativePreConstruct() override;

    UPROPERTY(Transient)
    USizeBox* root_size_box_{nullptr};
    UPROPERTY(Transient)
    UCanvasPanel* canvas_panel{nullptr};
    UPROPERTY(Transient)
    UBorder* background_widget{nullptr};
    UPROPERTY(Transient)
    UBorder* cursor_widget{nullptr};
    UPROPERTY(Transient)
    UTextBlock* name_text{nullptr};
    UPROPERTY(Transient)
    UTextBlock* value_text{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FText name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    bool show_value{true};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};
  private:
    void rebuild_widget_tree();
    void update_widgets();

    FVector2D value_{};
    TOptional<ml::ioj::FGameHudStyle> hud_style_{};
};
