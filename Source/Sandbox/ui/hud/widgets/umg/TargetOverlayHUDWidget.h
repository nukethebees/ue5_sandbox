#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "TargetOverlayHUDWidget.generated.h"

class UImage;

UCLASS()
class SANDBOX_API UTargetOverlayHUDWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    UPROPERTY(meta = (BindWidget))
    UImage* outline_top_left{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* outline_top_right{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* outline_bottom_left{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* outline_bottom_right{nullptr};
};
