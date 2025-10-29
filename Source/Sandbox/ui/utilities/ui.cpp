#include "Sandbox/ui/utilities/ui.h"

#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Engine/UserInterfaceSettings.h"

#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/logging/SandboxLogCategories.h"

namespace ml {
int32 scale_font_to_umg(int32 font_size) {
    constexpr float slate_dpi{96.0f};
    constexpr float default_umg_dpi{72.0f};

    if (auto* ui_settings{GetDefault<UUserInterfaceSettings>()}) {
        auto umg_dpi{72.0f};
        if (GEngine && GEngine->GameViewport->Viewport) {
            auto const size{GEngine->GameViewport->Viewport->GetSizeXY()};
            umg_dpi = ui_settings->GetDPIScaleBasedOnSize(size);
        }

        auto const dpi_scale_factor{umg_dpi / slate_dpi};
        return FMath::RoundToInt(static_cast<float>(font_size) * dpi_scale_factor);
    }

    return font_size;
}

auto get_grid_outer_coords(UGridPanel const& grid) -> FCoord {
    auto const slots{grid.GetSlots()};

    int32 max_row{-1};
    int32 max_col{-1};

    auto get_extra_span{[](int32 x) { return std::max(1, x) - 1; }};

    for (UPanelSlot* slot : slots) {
        if (auto* grid_slot{Cast<UGridSlot>(slot)}) {
            auto const row{grid_slot->GetRow()};
            auto const row_span{grid_slot->GetRowSpan()};
            auto const row_extra_span{get_extra_span(row_span)};

            auto const col{grid_slot->GetColumn()};
            auto const col_span{grid_slot->GetColumnSpan()};
            auto const col_extra_span{get_extra_span(col_span)};

            auto const outer_row{row + row_extra_span};
            auto const outer_col{col + col_extra_span};

            max_row = std::max(max_row, outer_row);
            max_col = std::max(max_col, outer_col);
        }
    }

    return FCoord{max_col, max_row};
}
}
