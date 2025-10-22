#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"

#include "Components/GridPanel.h"
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

    RETURN_IF_NULLPTR(inventory);
    RETURN_IF_NULLPTR(item_grid);
    RETURN_IF_NULLPTR(slot_class);
    RETURN_IF_NULLPTR(fallback_item_image);
    TRY_INIT_PTR(world, GetWorld());

    item_grid->ClearChildren();

    auto const n_rows{inventory->get_n_rows()};
    auto const n_cols{inventory->get_n_columns()};

    for (int32 col{0}; col < n_cols; ++col) {
        item_grid->SetColumnFill(col, 1.0f);
    }
    for (int32 row{0}; row < n_rows; ++row) {
        item_grid->SetRowFill(row, 1.0f);
    }

    for (int32 row{0}; row < n_rows; ++row) {
        for (int32 col{0}; col < n_cols; ++col) {
            auto* slot{CreateWidget<UInventorySlotWidget>(world, slot_class)};
            check(slot);

            slot->set_image(*fallback_item_image);

            item_grid->AddChildToGrid(slot, row, col);
        }
    }
}
