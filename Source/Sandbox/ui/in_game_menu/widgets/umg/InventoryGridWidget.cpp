#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"

#include "Components/GridPanel.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

#include "Sandbox/inventory/actor_components/InventoryComponent.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/InventorySlotWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInventoryGridWidget::NativeConstruct() {
    Super::NativeConstruct();
    log_verbose(TEXT("NativeConstruct."));

    OnNativeVisibilityChanged.AddUObject(this, &UInventoryGridWidget::on_visibility_changed);
    refresh_grid();
}
void UInventoryGridWidget::NativeDestruct() {
    log_verbose(TEXT("NativeDestruct."));
    OnNativeVisibilityChanged.RemoveAll(this);

    Super::NativeDestruct();
}
void UInventoryGridWidget::on_visibility_changed(ESlateVisibility new_visibility) {
    switch (new_visibility) {
        case ESlateVisibility::Visible: {
            refresh_grid();
            break;
        }
        default: {
            break;
        }
    }
}
void UInventoryGridWidget::refresh_grid() {
    log_verbose(TEXT("Refreshing grid."));

    RETURN_IF_NULLPTR(size_box);
    RETURN_IF_NULLPTR(inventory);
    RETURN_IF_NULLPTR(item_grid);
    RETURN_IF_NULLPTR(slot_class);
    RETURN_IF_NULLPTR(fallback_item_image);
    TRY_INIT_PTR(world, GetWorld());

    item_grid->ClearChildren();

    auto const n_rows{inventory->get_n_rows()};
    auto const n_cols{inventory->get_n_columns()};
    auto const n_slots{inventory->get_n_slots()};

    auto const grid_aspect_ratio{inventory->get_aspect_ratio()};
    size_box->SetMinAspectRatio(grid_aspect_ratio);
    size_box->SetMaxAspectRatio(grid_aspect_ratio);

    for (int32 col{0}; col < n_cols; ++col) {
        item_grid->SetColumnFill(col, 1.0f);
    }
    for (int32 row{0}; row < n_rows; ++row) {
        item_grid->SetRowFill(row, 1.0f);
    }

    TArray<bool> slot_filled;
    slot_filled.Init(false, n_slots);

    auto const items{inventory->get_slots_view()};
    for (auto const& item : items) {}

    // Add the empty slots
    for (int32 row{0}; row < n_rows; ++row) {
        for (int32 col{0}; col < n_cols; ++col) {
            auto const coord_id{row * n_cols + col};
            if (slot_filled[coord_id]) {
                continue;
            }

            auto const widget_name{FString::Printf(TEXT("slot_col_%d_row_%d"), col, row)};

            auto* slot{CreateWidget<UInventorySlotWidget>(world, slot_class, FName(*widget_name))};
            check(slot);

            slot->set_image(*fallback_item_image);
            slot->set_aspect_ratio(1.0f);

            item_grid->AddChildToGrid(slot, row, col);
        }
    }
}
