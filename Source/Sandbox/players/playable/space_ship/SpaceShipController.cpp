#include "Sandbox/players/playable/space_ship/SpaceShipController.h"

#include "Sandbox/players/playable/space_ship/SpaceShip.h"

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
}
void ASpaceShipController::Tick(float dt) {
    Super::Tick(dt);
}

void ASpaceShipController::BeginPlay() {
    Super::BeginPlay();
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
