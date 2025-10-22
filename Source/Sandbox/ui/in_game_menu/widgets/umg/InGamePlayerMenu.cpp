#include "Sandbox/ui/in_game_menu/widgets/umg/InGamePlayerMenu.h"

#include "Components/WidgetSwitcher.h"

#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/LogsMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/MapMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ObjectivesMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/PowersMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ResearchMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/StatsMenuWidget.h"
#include "Sandbox/ui/widgets/umg/TextButtonWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInGamePlayerMenu::NativeConstruct() {
    Super::NativeConstruct();

    WARN_IF_EXPR(!powers_tab_button);
    WARN_IF_EXPR(!stats_tab_button);
    WARN_IF_EXPR(!logs_tab_button);
    WARN_IF_EXPR(!objectives_tab_button);
    WARN_IF_EXPR(!map_tab_button);
    WARN_IF_EXPR(!research_tab_button);
    WARN_IF_EXPR(!inventory_tab_button);
    WARN_IF_EXPR(!close_button);

    // Bind tab button clicks
    if (powers_tab_button) {
        powers_tab_button->on_clicked.AddDynamic(this, &UInGamePlayerMenu::handle_powers_tab);
    }
    if (stats_tab_button) {
        stats_tab_button->on_clicked.AddDynamic(this, &UInGamePlayerMenu::handle_stats_tab);
    }
    if (logs_tab_button) {
        logs_tab_button->on_clicked.AddDynamic(this, &UInGamePlayerMenu::handle_logs_tab);
    }
    if (objectives_tab_button) {
        objectives_tab_button->on_clicked.AddDynamic(this,
                                                     &UInGamePlayerMenu::handle_objectives_tab);
    }
    if (map_tab_button) {
        map_tab_button->on_clicked.AddDynamic(this, &UInGamePlayerMenu::handle_map_tab);
    }
    if (research_tab_button) {
        research_tab_button->on_clicked.AddDynamic(this, &UInGamePlayerMenu::handle_research_tab);
    }
    if (inventory_tab_button) {
        inventory_tab_button->on_clicked.AddDynamic(this, &UInGamePlayerMenu::handle_inventory_tab);
    }

    // Bind close button click
    if (close_button) {
        close_button->on_clicked.AddDynamic(this, &UInGamePlayerMenu::handle_close);
    }

    // Set initial tab
    set_active_tab(EInGameMenuTab::Inventory);
}
void UInGamePlayerMenu::NativeDestruct() {
    if (powers_tab_button) {
        powers_tab_button->on_clicked.RemoveAll(this);
    }
    if (stats_tab_button) {
        stats_tab_button->on_clicked.RemoveAll(this);
    }
    if (logs_tab_button) {
        logs_tab_button->on_clicked.RemoveAll(this);
    }
    if (objectives_tab_button) {
        objectives_tab_button->on_clicked.RemoveAll(this);
    }
    if (map_tab_button) {
        map_tab_button->on_clicked.RemoveAll(this);
    }
    if (research_tab_button) {
        research_tab_button->on_clicked.RemoveAll(this);
    }
    if (inventory_tab_button) {
        inventory_tab_button->on_clicked.RemoveAll(this);
    }

    if (close_button) {
        close_button->on_clicked.RemoveAll(this);
    }

    Super::NativeDestruct();
}

void UInGamePlayerMenu::set_inventory(UInventoryComponent& inventory) {
    RETURN_IF_NULLPTR(inventory_tab);
    inventory_tab->set_inventory(inventory);
}

void UInGamePlayerMenu::handle_powers_tab() {
    set_active_tab(EInGameMenuTab::Powers);
}
void UInGamePlayerMenu::handle_stats_tab() {
    set_active_tab(EInGameMenuTab::Stats);
}
void UInGamePlayerMenu::handle_logs_tab() {
    set_active_tab(EInGameMenuTab::Logs);
}
void UInGamePlayerMenu::handle_objectives_tab() {
    set_active_tab(EInGameMenuTab::Objectives);
}
void UInGamePlayerMenu::handle_map_tab() {
    set_active_tab(EInGameMenuTab::Map);
}
void UInGamePlayerMenu::handle_research_tab() {
    set_active_tab(EInGameMenuTab::Research);
}
void UInGamePlayerMenu::handle_inventory_tab() {
    set_active_tab(EInGameMenuTab::Inventory);
    RETURN_IF_NULLPTR(inventory_tab);
    inventory_tab->on_widget_selected();
}
void UInGamePlayerMenu::handle_close() {
    back_requested.Broadcast();
}

void UInGamePlayerMenu::set_active_tab(EInGameMenuTab tab) {
    current_tab = tab;

    RETURN_IF_NULLPTR(tab_switcher);
    tab_switcher->SetActiveWidgetIndex(static_cast<int32>(tab));

    update_tab_button_states(tab);
}
void UInGamePlayerMenu::update_tab_button_states(EInGameMenuTab active_tab) {
    // Update button appearance to show which tab is active
    // Implementation depends on how TextButtonWidget handles selected states
}
void UInGamePlayerMenu::refresh() {
    constexpr auto logger{NestedLogger<"refresh">()};

    switch (current_tab) {
        using enum EInGameMenuTab;
        case Inventory: {
            check(inventory_tab);
            inventory_tab->on_widget_selected();
            break;
        }
        default: {
            logger.log_warning(TEXT("Unhandled case"));
            break;
        }
    }
}
