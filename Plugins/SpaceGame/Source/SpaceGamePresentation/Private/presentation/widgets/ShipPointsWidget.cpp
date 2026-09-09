#include "SpaceGamePresentation/presentation/widgets/ShipPointsWidget.h"

#include "SandboxGameShared/ui/widgets/ValueWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UShipPointsWidget::set_points(int32 points) {
    RETURN_IF_NULLPTR(widget);
    widget->update(points);
}

void UShipPointsWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    RETURN_IF_NULLPTR(widget);
    widget->set_format_spec(TEXT("SCORE // {0}"));
    widget->set_text_style(style.accent_text);
}

void UShipPointsWidget::set_font_size(int32 const new_font_size) {
    RETURN_IF_NULLPTR(widget);
    widget->set_font_size(new_font_size);
}

auto UShipPointsWidget::get_font_size() const noexcept -> int32 {
    return widget->get_font_size();
}
