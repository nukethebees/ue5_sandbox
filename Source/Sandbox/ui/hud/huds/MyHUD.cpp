#include "MyHUD.h"

#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/SWeakWidget.h"

#include "Sandbox/combat/weapons/actor_components/PawnWeaponComponent.h"
#include "Sandbox/environment/data/ActorCorners.h"
#include "Sandbox/game_flow/game_states/PlatformerGameState.h"
#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/inventory/actor_components/InventoryComponent.h"
#include "Sandbox/players/playable/actor_components/JetpackComponent.h"
#include "Sandbox/players/playable/player_controllers/MyPlayerController.h"
#include "Sandbox/ui/hud/widgets/umg/ItemDescriptionHUDWidget.h"
#include "Sandbox/ui/hud/widgets/umg/TargetOverlayHUDWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/slate/SInGameMenuWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/InGamePlayerMenu.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

template <typename T>
using CFinder = ConstructorHelpers::FClassFinder<T>;

AMyHUD::AMyHUD() {
    PrimaryActorTick.bCanEverTick = false;
}

void AMyHUD::BeginPlay() {
    Super::BeginPlay();

    RETURN_IF_NULLPTR(main_widget_class);
    TRY_INIT_PTR(world, GetWorld());

    if (!main_widget) {
        main_widget = CreateWidget<UMainHUDWidget>(world, main_widget_class);
        RETURN_IF_NULLPTR(main_widget_class);
        main_widget->AddToViewport();
    }

    if (auto* game_state{world->GetGameState<APlatformerGameState>()}) {
        game_state->on_coin_count_changed.AddDynamic(this, &AMyHUD::update_coin);
        update_coin(0);
    }

    TRY_INIT_PTR(player_controller, GetOwningPlayerController());
    TRY_INIT_PTR(player_pawn, player_controller->GetPawn());
    TRY_INIT_PTR(health_component, player_pawn->FindComponentByClass<UHealthComponent>());

    health_component->on_health_percent_changed.AddDynamic(this, &AMyHUD::update_health);

    TRY_INIT_PTR(jetpack_component, player_pawn->FindComponentByClass<UJetpackComponent>());
    jetpack_component->on_fuel_changed.AddDynamic(this, &AMyHUD::update_fuel);
    jetpack_component->broadcast_fuel_state();

    TRY_INIT_PTR(weapon_component, player_pawn->FindComponentByClass<UPawnWeaponComponent>());
    weapon_component->on_weapon_ammo_changed.AddDynamic(this, &AMyHUD::update_ammo);
    update_ammo({});
}

void AMyHUD::set_inventory(UInventoryComponent& inventory) {
    RETURN_IF_NULLPTR(umg_player_menu);
    umg_player_menu->set_inventory(inventory);
}

void AMyHUD::toggle_in_game_menu() {
    constexpr auto logger{NestedLogger<"toggle_in_game_menu">()};
    logger.log_display(TEXT("Toggling menu."));

    TRY_INIT_PTR(player_controller, GetOwningPlayerController());
    TRY_INIT_PTR(pc, Cast<AMyPlayerController>(player_controller));
    TRY_INIT_PTR(game_viewport, GetWorld()->GetGameViewport());

    if (use_umg_player_menu) {
        // UMG version
        if (is_in_game_menu_open) {
            RETURN_IF_NULLPTR(umg_player_menu);
            umg_player_menu->RemoveFromParent();
            pc->set_game_input_mode();
            is_in_game_menu_open = false;
        } else {
            if (!umg_player_menu) {
                TRY_INIT_PTR(world, GetWorld());
                umg_player_menu = CreateWidget<UInGamePlayerMenu>(world, umg_player_menu_class);
                RETURN_IF_NULLPTR(umg_player_menu);
                umg_player_menu->back_requested.AddDynamic(this, &AMyHUD::toggle_in_game_menu);

                TRY_INIT_PTR(pawn, player_controller->GetPawn());
                TRY_INIT_PTR(inventory_comp, pawn->FindComponentByClass<UInventoryComponent>());
                set_inventory(*inventory_comp);
            }

            RETURN_IF_NULLPTR(umg_player_menu);
            umg_player_menu->refresh();
            umg_player_menu->AddToViewport();
            pc->set_mouse_input_mode();
            is_in_game_menu_open = true;
        }
    } else {
        // Slate version
        if (is_in_game_menu_open) {
            if (in_game_menu_widget.IsValid()) {
                game_viewport->RemoveViewportWidgetContent(in_game_menu_widget.ToSharedRef());
            }
            pc->set_game_input_mode();
            is_in_game_menu_open = false;
        } else {
            if (!in_game_menu_widget.IsValid()) {
                // Get inventory component from player
                TRY_INIT_PTR(pawn, player_controller->GetPawn());
                TRY_INIT_PTR(inventory_comp, pawn->FindComponentByClass<UInventoryComponent>());

                in_game_menu_widget = SNew(SInGameMenuWidget)
                                          .OnExitClicked_Lambda([this]() {
                                              toggle_in_game_menu();
                                              return FReply::Handled();
                                          })
                                          .InventoryComponent(inventory_comp);
            }

            constexpr int32 z_order{100};
            game_viewport->AddViewportWidgetContent(in_game_menu_widget.ToSharedRef(), z_order);
            pc->set_mouse_input_mode();
            is_in_game_menu_open = true;
        }
    }
}
void AMyHUD::update_fuel(FJetpackState const& jetpack_state) {
    RETURN_IF_NULLPTR(main_widget);
    RETURN_IF_NULLPTR(main_widget->fuel_widget);

    main_widget->fuel_widget->update(jetpack_state.fuel_remaining);
}
void AMyHUD::update_jump(int32 new_jump) {
    RETURN_IF_NULLPTR(main_widget);
    RETURN_IF_NULLPTR(main_widget->jump_widget);

    main_widget->jump_widget->update(new_jump);
}
void AMyHUD::update_health(FHealthData health_data) {
    RETURN_IF_NULLPTR(main_widget);
    main_widget->update_health(health_data);
}
void AMyHUD::update_ammo(FAmmoData ammo_data) {
    RETURN_IF_NULLPTR(main_widget);
    RETURN_IF_NULLPTR(main_widget->ammo_widget);

    main_widget->ammo_widget->set_value(ammo_data.discrete_amount);
}
void AMyHUD::update_description(FText const& text) {
    RETURN_IF_NULLPTR(main_widget);
    RETURN_IF_NULLPTR(main_widget->item_description_widget);
    main_widget->item_description_widget->set_description(text);
}
void AMyHUD::update_target_screen_bounds(FActorCorners const& corners) {
    RETURN_IF_NULLPTR(main_widget);
    RETURN_IF_NULLPTR(main_widget->target_overlay);
    TRY_INIT_PTR(player_controller, GetOwningPlayerController());

    main_widget->target_overlay->update_target_screen_bounds(*player_controller, corners);
}
