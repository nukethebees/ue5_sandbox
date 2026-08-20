#include "ShooterGame/ui/ui.h"

#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "SandboxGameShared/utilities/actor_utils.h"

namespace ml {
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
