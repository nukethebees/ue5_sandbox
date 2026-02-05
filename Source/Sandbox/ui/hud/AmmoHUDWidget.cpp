#include "Sandbox/ui/hud/AmmoHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "Sandbox/combat/ammo/AmmoType.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UAmmoHUDWidget::set_max_ammo(FAmmoData new_max_ammo) {
    max_ammo = new_max_ammo;
}
void UAmmoHUDWidget::set_reserve_ammo(FAmmoData new_reserve_ammo) {
    static FText const fmt{FText::FromName(TEXT("[{0}]"))};

    check(reserve_ammo_text);

    reserve_ammo_text->SetText(FText::Format(fmt, new_reserve_ammo.amount));
}
void UAmmoHUDWidget::set_current_ammo(FAmmoData new_ammo) {
    static FText const fmt{FText::FromName(TEXT("{0} / {1}"))};

    check(ammo_text);
    check(progress_bar);

    float ammo_percent{0.0};
    FText text;

    text = FText::Format(fmt, new_ammo.amount, max_ammo.amount);

    RETURN_IF_TRUE(max_ammo.is_empty());
    RETURN_IF_TRUE(ml::is_continuous(max_ammo.type));
    ammo_percent = static_cast<float>(new_ammo.amount) / static_cast<float>(max_ammo.amount);

    ammo_text->SetText(text);
    progress_bar->SetPercent(ammo_percent);
}
