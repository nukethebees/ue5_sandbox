#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/TextBlock.h"

#include "Sandbox/combat/weapons/enums/AmmoType.h"
#include "Sandbox/inventory/actor_components/InventoryComponent.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ItemDetailsWidget.h"
#include "Sandbox/ui/utilities/ui.h"
#include "Sandbox/utilities/string.h"

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
    update_ammo_counts();
}
void UInventoryMenuWidget::update_money_display(int32 money) {
    RETURN_IF_NULLPTR(money_text);
    auto const text{FText::FromString(FString::Printf(TEXT("%d"), money))};
    money_text->SetText(text);
}
void UInventoryMenuWidget::update_ammo_counts() {
    auto n_ammo_types{static_cast<int32>(ml::ammo_types.size())};
    check(inventory);

    for (int32 i{0}; i < n_ammo_types; i++) {
        auto const ammo_type{ml::ammo_types[i]};
        auto const count(inventory->count_ammo(ammo_type));

        ammo_counts[i]->SetText(FText::AsNumber(count.amount));
    }
}

void UInventoryMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto const outer_coords{ml::get_grid_outer_coords(*numbers_list_panel)};

    int32 row{outer_coords.y() + 1};

    auto create_slot{[&](int32 i) {
        constexpr int32 label_column{0};
        constexpr int32 value_column{1};

        auto const value{ml::ammo_types[i]};

        auto const label_name{FString::Printf(TEXT("ammo_label_%d"), row)};
        auto const label_text{
            FString::Printf(TEXT("%s"), *ml::without_class_prefix(UEnum::GetValueAsString(value)))};

        auto* label_widget{
            WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *label_name)};
        label_widget->SetText(FText::FromString(label_text));
        label_widget->SetJustification(ETextJustify::Left);

        auto* label_slot{numbers_list_panel->AddChildToGrid(label_widget, row, label_column)};
        label_slot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
        label_slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);

        auto const count_name{FString::Printf(TEXT("ammo_count_%d"), row)};

        auto* count_widget{
            WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *count_name)};
        count_widget->SetText(FText::FromString(TEXT("0")));
        count_widget->SetJustification(ETextJustify::Right);

        auto* count_slot{numbers_list_panel->AddChildToGrid(count_widget, row, value_column)};
        count_slot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
        count_slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);

        ammo_counts.Add(count_widget);

        row++;
    }};

    auto n_ammo_types{static_cast<int32>(ml::ammo_types.size())};
    for (int32 i{0}; i < n_ammo_types; i++) {
        create_slot(i);
    }
}
void UInventoryMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}
