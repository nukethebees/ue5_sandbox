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
    initialise_hud();
}

void ASpaceShipController::OnPossess(APawn* in_pawn) {
    Super::OnPossess(in_pawn);

    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));
    subsystem->AddMappingContext(input.base_context, 0);

    initialise_hud();

    RETURN_IF_NULLPTR(hud_widget);
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(in_pawn));

    ship->on_health_changed.BindUObject(hud_widget, &UShipHudWidget::set_health);
    hud_widget->set_health(1.f);
    ship->on_speed_changed.BindUObject(hud_widget, &UShipHudWidget::set_speed);
    hud_widget->set_speed(ship->get_speed());
    ship->on_energy_changed.BindUObject(hud_widget, &UShipHudWidget::set_energy);
    hud_widget->set_energy(1.f);
    ship->on_bombs_changed.BindUObject(hud_widget, &UShipHudWidget::set_bombs);
    hud_widget->set_bombs(ship->get_bombs());
    ship->on_gold_rings_changed.BindUObject(hud_widget, &UShipHudWidget::set_gold_rings);
    hud_widget->set_gold_rings(ship->get_gold_rings());
    ship->on_points_changed.BindUObject(hud_widget, &UShipHudWidget::set_points);
    hud_widget->set_points(ship->get_points());
    ship->on_lives_changed.BindUObject(hud_widget, &UShipHudWidget::set_lives);
    hud_widget->set_lives(ship->get_lives());

    ship->SetActorTickEnabled(true);
}
void ASpaceShipController::OnUnPossess() {
    if (auto* ship{Cast<ASpaceShip>(GetPawn())}) {
        ship->SetActorTickEnabled(false);

        ship->on_health_changed.Unbind();
        ship->on_speed_changed.Unbind();
        ship->on_energy_changed.Unbind();
        ship->on_bombs_changed.Unbind();
        ship->on_gold_rings_changed.Unbind();
        ship->on_points_changed.Unbind();
        ship->on_lives_changed.Unbind();
    }

    Super::OnUnPossess();
}
void ASpaceShipController::initialise_hud() {
    if (hud_widget) {
        return;
    }

    RETURN_IF_NULLPTR(hud_widget_class);
    hud_widget = CreateWidget<UShipHudWidget>(this, hud_widget_class, TEXT("ship_hud"));
    RETURN_IF_NULLPTR(hud_widget);
    hud_widget->AddToViewport();
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
