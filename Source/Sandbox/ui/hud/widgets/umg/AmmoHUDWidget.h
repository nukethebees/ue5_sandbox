#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/combat/weapons/data/AmmoData.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "AmmoHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class SANDBOX_API UAmmoHUDWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"AmmoHUDWidget", LogSandboxUI> {
    GENERATED_BODY()
  public:
    void set_max_ammo(FAmmoData new_max_ammo);
    void set_current_ammo(FAmmoData new_ammo);
  protected:
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* progress_bar;
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* ammo_text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    FAmmoData max_ammo{};
};
