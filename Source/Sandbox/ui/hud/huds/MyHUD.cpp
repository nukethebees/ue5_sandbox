#include "MyHUD.h"

#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

#include "Sandbox/game_flow/game_states/PlatformerGameState.h"
#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/players/playable/actor_components/JetpackComponent.h"

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
