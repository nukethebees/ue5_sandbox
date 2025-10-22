#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"

#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

#include "Sandbox/inventory/actor_components/InventoryComponent.h"
#include "Sandbox/ui/in_game_menu/misc/InventorySlotDragDropOperation.h"
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

bool UInventoryGridWidget::NativeOnDrop(FGeometry const& InGeometry,
                                        FDragDropEvent const& InDragDropEvent,
                                        UDragDropOperation* InOperation) {
    constexpr auto logger{NestedLogger<"NativeOnDrop">()};

    logger.log_verbose(TEXT("Start"));
    if (auto* op{Cast<UInventorySlotDragDropOperation>(InOperation)}) {
        logger.log_verbose(TEXT("Cast successful."));
        check(op->inventory_slot);

        auto const grid_geometry{item_grid->GetTickSpaceGeometry()};
        auto const widget_size{grid_geometry.GetLocalSize()};
        auto const mouse_pos{
            grid_geometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition())};

        auto const dimensions{inventory->get_dimensions()};
        auto const w{static_cast<float>(dimensions.x())};
        auto const h{static_cast<float>(dimensions.y())};

        auto const cell_width{widget_size.X / w};
        auto const cell_height{widget_size.Y / h};

        auto const click_col{static_cast<int32>(mouse_pos.X / cell_width)};
        auto const click_row{static_cast<int32>(mouse_pos.Y / cell_height)};

        FCoord drop_location{click_col, click_row};

        if (inventory->move_item(*op->inventory_slot, op->click_local_location, drop_location)) {
            refresh_grid();
            return true;
        }
    }

    return false;
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

    constexpr auto make_name{[](auto col, auto row) {
        auto const str{FString::Printf(TEXT("slot_col_%d_row_%d"), col, row)};
        return FName(*str);
    }};

    auto const items{inventory->get_slots_view()};
    for (auto const& item : items) {
        check(item.item);

        auto const item_origin{item.origin};
        auto const origin_col{item_origin.x()};
        auto const origin_row{item_origin.y()};

        auto* slot_widget{CreateWidget<UInventorySlotWidget>(
            world, slot_class, make_name(origin_col, origin_row))};
        check(slot_widget);
        slot_widget->set_inventory_slot(item);

        auto const w{item.width()};
        auto const h{item.height()};

        auto* grid_slot{item_grid->AddChildToGrid(slot_widget, origin_row, origin_col)};
        check(grid_slot);
        grid_slot->SetRowSpan(h);
        grid_slot->SetColumnSpan(w);

        for (int32 x{0}; x < w; ++x) {
            for (int32 y{0}; y < h; ++y) {
                auto const row{origin_row + y};
                auto const col{origin_col + x};

                auto const coord_id{row * n_cols + col};
                slot_filled[coord_id] = true;
            }
        }
    }

    // Add the empty slots
    for (int32 row{0}; row < n_rows; ++row) {
        for (int32 col{0}; col < n_cols; ++col) {
            auto const coord_id{row * n_cols + col};
            if (slot_filled[coord_id]) {
                continue;
            }

            auto* slot{CreateWidget<UInventorySlotWidget>(world, slot_class, make_name(col, row))};
            check(slot);

            slot->set_image(*fallback_item_image);
            slot->set_aspect_ratio(1.0f);

            item_grid->AddChildToGrid(slot, row, col);
        }
    }
}
