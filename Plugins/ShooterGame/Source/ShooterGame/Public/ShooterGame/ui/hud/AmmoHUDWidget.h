#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ShooterGame/combat/ammo/AmmoData.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "AmmoHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class SHOOTERGAME_API UAmmoHUDWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"AmmoHUDWidget", LogShooterGameUI> {
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
