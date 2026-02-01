#include "Sandbox/players/playable/space_ship/SpaceShipController.h"

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
    bind(input.move, Triggered, &ThisClass::move);
}
void ASpaceShipController::Tick(float dt) {
    Super::Tick(dt);
}

void ASpaceShipController::BeginPlay() {
    Super::BeginPlay();
}
void ASpaceShipController::move(FInputActionValue const& value) {}
