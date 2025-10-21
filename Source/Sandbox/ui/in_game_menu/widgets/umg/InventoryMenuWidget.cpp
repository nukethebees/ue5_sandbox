#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryMenuWidget.h"

#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ItemDetailsWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInventoryMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}
void UInventoryMenuWidget::on_widget_selected() {
    RETURN_IF_NULLPTR(inventory_grid);
    inventory_grid->refresh_grid();
}
