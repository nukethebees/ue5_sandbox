#include "SpaceGame/presentation/widgets/ShipHealthWidget.h"

#include "SandboxGameShared/ui/widgets/ValueWidget.h"

#include "Components/ProgressBar.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UShipHealthWidget::set_health(FShipHealth health) {
    check(health_bar);
    check(health_text);

    auto const percent{health.max_health > 0 ? static_cast<float>(health.health) /
                                                   static_cast<float>(health.max_health)
                                             : 0.0f};
    health_bar->SetPercent(percent);

    if (hud_style_) {
        auto const& style{hud_style_.GetValue()};
        auto const colour{percent <= style.health_critical_threshold  ? style.health_critical
                          : percent <= style.health_warning_threshold ? style.health_warning
                                                                      : style.health_nominal};
        health_bar->SetFillColorAndOpacity(colour);
    }

    health_text->update(health.health, health.max_health);
}

void UShipHealthWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    if (health_bar) {
        health_bar->SetWidgetStyle(style.health_bar);
    }
    if (health_text) {
        health_text->set_text_style(style.primary_text);
    }
}

void UShipHealthWidget::set_font_size(int32 const new_font_size) {
    check(health_text);
    health_text->set_font_size(new_font_size);
}

auto UShipHealthWidget::get_font_size() const noexcept -> int32 {
    return health_text->get_font_size();
}
