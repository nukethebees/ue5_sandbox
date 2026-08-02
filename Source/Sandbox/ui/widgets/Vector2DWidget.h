#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Vector2DWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UVector2DWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void NativeConstruct() override;

    void update(FVector2D const value);
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* value_widget{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FName name;
};
