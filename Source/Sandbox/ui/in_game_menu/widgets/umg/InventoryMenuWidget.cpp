#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryMenuWidget.h"

#include "Components/TextBlock.h"

#include "Sandbox/inventory/actor_components/InventoryComponent.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ItemDetailsWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInventoryMenuWidget::set_inventory(UInventoryComponent& inv) {
    RETURN_IF_NULLPTR(inventory_grid);
    inventory = &inv;
    inventory_grid->set_inventory(inv);
    update_money_display(inv.get_money());
}
void UInventoryMenuWidget::on_widget_selected() {
    RETURN_IF_NULLPTR(inventory_grid);
    inventory_grid->refresh_grid();

    RETURN_IF_NULLPTR(inventory);
    update_money_display(inventory->get_money());
}

void UInventoryMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}

void UInventoryMenuWidget::update_money_display(int32 money) {
    RETURN_IF_NULLPTR(money_text);
    auto const text{FText::FromString(FString::Printf(TEXT("Money: %d"), money))};
    money_text->SetText(text);
}
