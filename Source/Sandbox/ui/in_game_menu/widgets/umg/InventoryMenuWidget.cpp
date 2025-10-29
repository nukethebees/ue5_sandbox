#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/TextBlock.h"

#include "Sandbox/combat/weapons/enums/AmmoType.h"
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
void UInventoryMenuWidget::update_money_display(int32 money) {
    RETURN_IF_NULLPTR(money_text);
    auto const text{FText::FromString(FString::Printf(TEXT("%d"), money))};
    money_text->SetText(text);
}

void UInventoryMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    int32 row{1};
    FString widget_name;
    auto update_widget_name{[&]() -> FString const& {
        widget_name = FString::Printf(TEXT("ammo_%d"), row);
        return widget_name;
    }};
    auto create_slot{[&](int32 i) {
        auto const value{static_cast<EAmmoType>(i)};

        auto* item{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
                                                           *update_widget_name())};
        auto const row_name{FString::Printf(TEXT("%s"), *UEnum::GetValueAsString(value))};

        item->SetText(FText::FromString(row_name));
        auto* slot{numbers_list_panel->AddChildToGrid(item, row, 0)};
        slot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
        slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
        row++;
    }};

    for (int32 i{ammo_type_discrete_begin()}; i < ammo_type_discrete_end(); ++i) {
        create_slot(i);
    }
    for (int32 i{ammo_type_continuous_begin()}; i < ammo_type_continuous_end(); ++i) {
        create_slot(i);
    }
}
void UInventoryMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}
