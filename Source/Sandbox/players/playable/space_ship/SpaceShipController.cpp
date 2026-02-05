#include "Sandbox/players/playable/space_ship/SpaceShipController.h"

#include "Sandbox/players/playable/space_ship/SpaceShip.h"
#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ASpaceShipController::ASpaceShipController() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ASpaceShipController::SetupInputComponent() {
    Super::SetupInputComponent();

    TRY_INIT_PTR(eic, Cast<UEnhancedInputComponent>(InputComponent));
    auto bind{make_input_binder(eic)};

    using enum ETriggerEvent;

    // Movement
    bind(input.turn, Triggered, &ThisClass::turn);
    bind(input.turn, Completed, &ThisClass::turn_completed);
    bind(input.fire_laser, Started, &ThisClass::fire_laser);
    bind(input.fire_bomb, Started, &ThisClass::fire_bomb);
    bind(input.boost, Triggered, &ThisClass::boost);
    bind(input.brake, Triggered, &ThisClass::brake);
}
void ASpaceShipController::Tick(float dt) {
    Super::Tick(dt);
}

void ASpaceShipController::BeginPlay() {
    Super::BeginPlay();

    RETURN_IF_NULLPTR(hud_widget_class);
    hud_widget = CreateWidget<UShipHudWidget>(this, hud_widget_class);
    RETURN_IF_NULLPTR(hud_widget);
    hud_widget->AddToViewport();
}

void ASpaceShipController::OnPossess(APawn* in_pawn) {
    Super::OnPossess(in_pawn);

    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));
    subsystem->AddMappingContext(input.base_context, 0);
}
void ASpaceShipController::turn(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->turn(value.Get<FVector2D>());
}
void ASpaceShipController::turn_completed(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->turn(FVector2D::ZeroVector);
}
void ASpaceShipController::fire_laser(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->fire_laser();
}
void ASpaceShipController::fire_bomb(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->fire_bomb();
}
void ASpaceShipController::boost(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->boost();
}
void ASpaceShipController::brake(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->brake();
}
