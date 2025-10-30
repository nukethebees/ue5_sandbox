#include "Sandbox/ui/hud/widgets/umg/AmmoHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "Sandbox/combat/weapons/enums/AmmoType.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UAmmoHUDWidget::set_max_ammo(FAmmoData new_max_ammo) {
    max_ammo = new_max_ammo;
}
void UAmmoHUDWidget::set_current_ammo(FAmmoData new_ammo) {
    float ammo_percent{0.0};

    if (is_continuous(new_ammo.type)) {
        ammo_text->SetText(FText::AsNumber(new_ammo.continuous_amount));
        RETURN_IF_TRUE(max_ammo.is_empty());
        RETURN_IF_TRUE(!is_continuous(max_ammo.type));

        ammo_percent = new_ammo.continuous_amount / max_ammo.continuous_amount;
    } else {
        ammo_text->SetText(FText::AsNumber(new_ammo.discrete_amount));
        RETURN_IF_TRUE(max_ammo.is_empty());
        RETURN_IF_TRUE(is_continuous(max_ammo.type));

        ammo_percent = static_cast<float>(new_ammo.discrete_amount) /
                       static_cast<float>(max_ammo.discrete_amount);
    }

    progress_bar->SetPercent(ammo_percent);
}
