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
#include "Sandbox/utilities/enums.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInGamePlayerMenu::NativeOnInitialized() {
    Super::NativeOnInitialized();

#define BIND_CLICKED(BUTTON, FUNCTION)                                         \
    do {                                                                       \
        if (BUTTON) {                                                          \
            BUTTON->on_clicked.AddUObject(this, &UInGamePlayerMenu::FUNCTION); \
        } else {                                                               \
            WARN_IF_EXPR(BUTTON);                                              \
        }                                                                      \
    } while (0)

    BIND_CLICKED(powers_tab_button, handle_powers_tab);
    BIND_CLICKED(stats_tab_button, handle_stats_tab);
    BIND_CLICKED(logs_tab_button, handle_logs_tab);
    BIND_CLICKED(objectives_tab_button, handle_objectives_tab);
    BIND_CLICKED(map_tab_button, handle_map_tab);
    BIND_CLICKED(research_tab_button, handle_research_tab);
    BIND_CLICKED(inventory_tab_button, handle_inventory_tab);
    BIND_CLICKED(close_button, handle_close);

#undef BIND_CLICKED
}
void UInGamePlayerMenu::NativeConstruct() {
    Super::NativeConstruct();
    set_active_tab(EInGameMenuTab::Inventory);
}
void UInGamePlayerMenu::NativeDestruct() {
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
        case Powers: {
            break;
        }
        case Stats: {
            break;
        }
        case Objectives: {
            break;
        }
        case Map: {
            break;
        }
        case Logs: {
            break;
        }
        case Research: {
            break;
        }
        default: {
            logger.log_warning(TEXT("%s"), *ml::make_unhandled_enum_case_warning(current_tab));
            break;
        }
    }
}
