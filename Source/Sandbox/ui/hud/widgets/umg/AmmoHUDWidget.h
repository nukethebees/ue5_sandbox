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
    void set_reserve_ammo(FAmmoData new_reserve_ammo);
    void set_current_ammo(FAmmoData new_ammo);
  protected:
    UPROPERTY(meta = (BindWidget))
    UProgressBar* progress_bar;
    UPROPERTY(meta = (BindWidget))
    UTextBlock* ammo_text;
    UPROPERTY(meta = (BindWidget))
    UTextBlock* reserve_ammo_text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    FAmmoData max_ammo{};
};
