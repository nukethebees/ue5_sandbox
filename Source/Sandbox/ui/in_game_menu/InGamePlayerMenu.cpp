#include "Sandbox/ui/in_game_menu/InGamePlayerMenu.h"

#include "Components/WidgetSwitcher.h"

#include "Sandbox/ui/in_game_menu/InventoryMenuWidget.h"
#include "Sandbox/ui/in_game_menu/LogsMenuWidget.h"
#include "Sandbox/ui/in_game_menu/MapMenuWidget.h"
#include "Sandbox/ui/in_game_menu/ObjectivesMenuWidget.h"
#include "Sandbox/ui/in_game_menu/PowersMenuWidget.h"
#include "Sandbox/ui/in_game_menu/ResearchMenuWidget.h"
#include "Sandbox/ui/in_game_menu/StatsMenuWidget.h"
#include "Sandbox/ui/widgets/TextButtonWidget.h"
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
void UInGamePlayerMenu::set_character(AMyCharacter& character) {
    RETURN_IF_NULLPTR(stats_tab);
    stats_tab->set_character(character);
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

    refresh();
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
            check(powers_tab);
            powers_tab->on_widget_selected();
            break;
        }
        case Stats: {
            check(stats_tab);
            stats_tab->on_widget_selected();
            break;
        }
        case Objectives: {
            check(objectives_tab);
            objectives_tab->on_widget_selected();
            break;
        }
        case Map: {
            check(map_tab);
            map_tab->on_widget_selected();
            break;
        }
        case Logs: {
            check(logs_tab);
            logs_tab->on_widget_selected();
            break;
        }
        case Research: {
            check(research_tab);
            research_tab->on_widget_selected();
            break;
        }
        default: {
            logger.log_warning(TEXT("%s"), *ml::make_unhandled_enum_case_warning(current_tab));
            break;
        }
    }
}
