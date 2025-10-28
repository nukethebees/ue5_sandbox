#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "TargetOverlayHUDWidget.generated.h"

class UImage;

struct FActorCorners;

UCLASS()
class SANDBOX_API UTargetOverlayHUDWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UTargetOverlayHUDWidget", LogSandboxUI> {
    GENERATED_BODY()
  public:
    void update_target_screen_bounds(APlayerController& pc, FActorCorners const& corners);
    void hide();
  protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget))
    UImage* outline_top_left{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* outline_top_right{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* outline_bottom_left{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* outline_bottom_right{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
    float outline_padding{100.0f};

    TStaticArray<UImage*, 4> corners_;
};
