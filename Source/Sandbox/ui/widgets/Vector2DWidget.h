#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Vector2DWidget.generated.h"

class UBorder;
class UCanvasPanel;

UCLASS()
class SANDBOX_API UVector2DWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void NativeConstruct() override;

    void update(FVector2D const value);
  protected:
    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* canvas_panel{nullptr};
    UPROPERTY(meta = (BindWidget))
    UBorder* background_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UBorder* cursor_widget{nullptr};
};
