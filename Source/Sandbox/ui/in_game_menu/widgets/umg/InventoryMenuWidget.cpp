#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryMenuWidget.h"

#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ItemDetailsWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInventoryMenuWidget::set_inventory(UInventoryComponent& inv) {
    RETURN_IF_NULLPTR(inventory_grid);
    inventory_grid->set_inventory(inv);
}
void UInventoryMenuWidget::on_widget_selected() {
    RETURN_IF_NULLPTR(inventory_grid);
    inventory_grid->refresh_grid();
}

void UInventoryMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}
