#include "MyHUD.h"

#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/SWeakWidget.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/actor_components/JetpackComponent.h"
#include "Sandbox/game_states/PlatformerGameState.h"
#include "Sandbox/player_controllers/MyPlayerController.h"
#include "Sandbox/slate_widgets/SInGameMenuWidget.h"

#include "Sandbox/macros/null_checks.hpp"

template <typename T>
using CFinder = ConstructorHelpers::FClassFinder<T>;

AMyHUD::AMyHUD() {
    PrimaryActorTick.bCanEverTick = false;
}

void AMyHUD::BeginPlay() {
    Super::BeginPlay();

    if (!main_widget_class) {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: No main_widget_class."));
        return;
    }

    if (!main_widget) {
        main_widget = CreateWidget<UMainHUDWidget>(GetWorld(), main_widget_class);
        if (!main_widget) {
            return;
        }
        main_widget->AddToViewport();
    }

    if (auto* game_state{GetWorld()->GetGameState<APlatformerGameState>()}) {
        game_state->on_coin_count_changed.AddDynamic(this, &AMyHUD::update_coin);
        update_coin(0);
    }

    auto* player_controller{GetOwningPlayerController()};
    if (!player_controller) {
        return;
    }

    auto player_pawn{player_controller->GetPawn()};
    if (!player_pawn) {
        return;
    }

    auto* health_component{player_pawn->FindComponentByClass<UHealthComponent>()};
    if (!health_component) {
        UE_LOG(LogTemp, Warning, TEXT("AMyHUD: HealthComponent not found on player pawn."));
        return;
    }
    health_component->on_health_percent_changed.AddDynamic(this, &AMyHUD::update_health);

    auto* jetpack_component{player_pawn->FindComponentByClass<UJetpackComponent>()};
    if (!jetpack_component) {
        UE_LOG(LogTemp, Warning, TEXT("AMyHUD: UJetpackComponent not found on player pawn."));
        return;
    }
    jetpack_component->on_fuel_changed.AddDynamic(this, &AMyHUD::update_fuel);
    jetpack_component->broadcast_fuel_state();
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
void AMyHUD::update_ammo(int32 ammo_count) {
    RETURN_IF_NULLPTR(main_widget);
    RETURN_IF_NULLPTR(main_widget->ammo_widget);

    main_widget->ammo_widget->set_value(ammo_count);
}

void AMyHUD::toggle_in_game_menu() {
    constexpr auto logger{NestedLogger<"toggle_in_game_menu">()};
    logger.log_display(TEXT("Toggling menu."));

    TRY_INIT_PTR(player_controller, GetOwningPlayerController());
    TRY_INIT_PTR(pc, Cast<AMyPlayerController>(player_controller));
    TRY_INIT_PTR(game_viewport, GetWorld()->GetGameViewport());

    if (is_in_game_menu_open) {
        if (in_game_menu_widget.IsValid()) {
            game_viewport->RemoveViewportWidgetContent(in_game_menu_widget.ToSharedRef());
        }
        pc->set_game_input_mode();
        is_in_game_menu_open = false;
    } else {
        if (!in_game_menu_widget.IsValid()) {
            in_game_menu_widget = SNew(SInGameMenuWidget).OnExitClicked_Lambda([this]() {
                toggle_in_game_menu();
                return FReply::Handled();
            });
        }

        constexpr int32 z_order{100};
        game_viewport->AddViewportWidgetContent(in_game_menu_widget.ToSharedRef(), z_order);
        pc->set_mouse_input_mode();
        is_in_game_menu_open = true;
    }
}
