#include "Sandbox/ui/ship_hud/ShipBombCountWidget.h"

#include "Sandbox/ui/ship_hud/ShipRenderIconWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Spacer.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipBombCountWidget::set_count(int32 count) {
    constexpr int32 max_icons{8};
    constexpr int32 columns{4};

    RETURN_IF_NULLPTR(grid);
    TRY_INIT_PTR(world, GetWorld());

    grid->ClearChildren();

    auto const icons_to_show{FMath::Min(count, max_icons)};

    for (int32 i{0}; i < icons_to_show; ++i) {
        auto* icon_widget{CreateWidget<UShipRenderIconWidget>(world, icon_widget_class)};
        check(icon_widget);

        auto const row{i / columns};
        auto const column{i % columns};

        auto* slot{grid->AddChildToUniformGrid(icon_widget, row, column)};
        check(slot);

        slot->SetHorizontalAlignment(HAlign_Fill);
        slot->SetVerticalAlignment(VAlign_Fill);
    }

    for (int32 i{icons_to_show}; i < max_icons; ++i) {
        auto* spacer{WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())};
        auto const row{i / columns};
        auto const column{i % columns};
        auto* slot{grid->AddChildToUniformGrid(spacer, row, column)};
        slot->SetHorizontalAlignment(HAlign_Fill);
        slot->SetVerticalAlignment(VAlign_Fill);
    }
}
