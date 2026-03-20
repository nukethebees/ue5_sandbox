#include "Sandbox/players/SpaceShipController.h"

#include "Sandbox/health/ShipHealthComponent.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/SpaceShip.h"
#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "DrawDebugHelpers.h"
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
    bind(input.roll, Started, &ThisClass::start_roll);
    bind(input.roll, Triggered, &ThisClass::roll);
    bind(input.roll, Completed, &ThisClass::stop_roll);
    bind(input.barrel_roll, Triggered, &ThisClass::barrel_roll);
    bind(input.boost, Started, &ThisClass::start_boost);
    bind(input.boost, Completed, &ThisClass::stop_boost);
    bind(input.brake, Started, &ThisClass::start_brake);
    bind(input.brake, Completed, &ThisClass::stop_brake);

    bind(input.fire_laser, Started, &ThisClass::start_fire_laser);
    bind(input.fire_laser, Completed, &ThisClass::stop_fire_laser);
    bind(input.fire_bomb, Started, &ThisClass::fire_bomb);
}
void ASpaceShipController::Tick(float dt) {
    Super::Tick(dt);

    RETURN_IF_NULLPTR(hud_widget);
    TRY_INIT_PTR(ss, Cast<ASpaceShip>(GetPawn()));

    auto const ship_socket{ss->get_middle_socket()};

    auto const ship_loc{ship_socket.GetLocation()};
    auto const ship_fwd{ship_socket.GetUnitAxis(EAxis::X)};
    auto const near_world_pos{ship_loc + ship_fwd * near_cursor_distance};
    auto const far_world_pos{ship_loc + ship_fwd * far_cursor_distance};
    FVector2d near_screen_pos{};
    FVector2d far_screen_pos{};

    constexpr bool bPlayerViewportRelative{false};
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            this, near_world_pos, near_screen_pos, bPlayerViewportRelative)) {
        UE_LOG(LogSandboxController, Warning, TEXT("Failed to project near position."));
    }

    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            this, far_world_pos, far_screen_pos, bPlayerViewportRelative)) {
        UE_LOG(LogSandboxController, Warning, TEXT("Failed to project far position."));
    }

    hud_widget->set_crosshair_positions(near_screen_pos, far_screen_pos);

#if WITH_EDITOR
    if (debug_crosshair) {
        TRY_INIT_PTR(world, GetWorld());
        DrawDebugSphere(world, near_world_pos, 50.f, 12, FColor::Green, false, 0.f);
        DrawDebugSphere(world, far_world_pos, 50.f, 12, FColor::Green, false, 0.f);

        if (can_log()) {
            UE_LOG(LogSandboxController,
                   Verbose,
                   TEXT("Near (W): %s"),
                   *near_world_pos.ToCompactString());
            UE_LOG(
                LogSandboxController, Verbose, TEXT("Near (S): %s"), *near_screen_pos.ToString());
            UE_LOG(LogSandboxController,
                   Verbose,
                   TEXT("Far (W): %s"),
                   *far_world_pos.ToCompactString());
            UE_LOG(LogSandboxController, Verbose, TEXT("Far (S): %s"), *far_screen_pos.ToString());
        }
    }

    if (can_log()) {
        seconds_since_last_log = 0.f;
    }

    seconds_since_last_log += dt;
#endif
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

    auto& health_delegate{ship->get_on_health_changed_delegate()};
    health_delegate.BindUObject(hud_widget, &UShipHudWidget::set_health);
    hud_widget->set_health(ship->get_health_info());
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
    ship->on_laser_mode_changed.BindUObject(this, &ThisClass::on_laser_firing_mode_changed);
    on_laser_firing_mode_changed(ELaserFiringMode::idle);

#if WITH_EDITOR
    ship->on_speed_sampled.BindUObject(hud_widget, &UShipHudWidget::update_sampled_speed);
#endif

    ship->SetActorTickEnabled(true);
}
void ASpaceShipController::OnUnPossess() {
    if (auto* ship{Cast<ASpaceShip>(GetPawn())}) {
        ship->SetActorTickEnabled(false);

        ship->get_on_health_changed_delegate().Unbind();
        ship->on_speed_changed.Unbind();
        ship->on_energy_changed.Unbind();
        ship->on_bombs_changed.Unbind();
        ship->on_gold_rings_changed.Unbind();
        ship->on_points_changed.Unbind();
        ship->on_lives_changed.Unbind();

#if WITH_EDITOR
        ship->on_speed_sampled.Unbind();
#endif
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
void ASpaceShipController::start_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("Begin roll: %.1f"), value.Get<float>());
}
void ASpaceShipController::roll(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));

    auto& br{barrel_roll_input};

    auto const r{value.Get<float>()};

    if ((FMath::Abs(r) >= br.input_strength_threshold) && !br.threshold_crossed_this_input) {
        auto const last_crossing_time{br.last_crossing_time};

        TRY_INIT_PTR(world, GetWorld());
        br.last_crossing_time = world->GetTimeSeconds();
        br.threshold_crossed_this_input = true;

        UE_LOG(LogSandboxController,
               Verbose,
               TEXT("Barrel roll threshold crossed: %.2f"),
               br.last_crossing_time);

        auto const delta_time{br.last_crossing_time - last_crossing_time};
        auto const begin_barrel_roll{delta_time <= br.input_time_threshold};

        if (begin_barrel_roll) {
            UE_LOG(LogSandboxController,
                   Verbose,
                   TEXT("Barrel roll: %.1f\n    Prev: %.2f\n    Cur: %.2f\n    Delta: %.2f"),
                   r,
                   last_crossing_time,
                   br.last_crossing_time,
                   delta_time);
            ship->barrel_roll(r);
        }
    } else {
        ship->roll(r);
    }
}
void ASpaceShipController::stop_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("End roll: %.1f"), value.Get<float>());
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->roll(0.f);
    barrel_roll_input.threshold_crossed_this_input = false;
}
void ASpaceShipController::barrel_roll(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->barrel_roll(value.Get<float>());
}
void ASpaceShipController::start_boost(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->start_boost();
}
void ASpaceShipController::stop_boost(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->stop_boost();
}
void ASpaceShipController::start_brake(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->start_brake();
}
void ASpaceShipController::stop_brake(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->stop_brake();
}

void ASpaceShipController::start_fire_laser() {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->start_fire_laser();
}
void ASpaceShipController::stop_fire_laser() {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->stop_fire_laser();
}
void ASpaceShipController::fire_bomb(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<ASpaceShip>(GetPawn()));
    ship->fire_bomb();
}

void ASpaceShipController::on_laser_firing_mode_changed(ELaserFiringMode mode) {
    RETURN_IF_NULLPTR(hud_widget);

    switch (mode) {
        case ELaserFiringMode::lock_on: {
            hud_widget->set_crosshair_colours(FLinearColor::Yellow, FLinearColor::Red);
            break;
        }
        default: {
            UE_LOG(LogSandboxController, Warning, TEXT("Unhandled laser firing mode."));
            [[fallthrough]];
        }
        case ELaserFiringMode::idle:
            [[fallthrough]];
        case ELaserFiringMode::lock_on_transition:
            [[fallthrough]];
        case ELaserFiringMode::burst: {
            hud_widget->set_crosshair_colours(FLinearColor::Green, FLinearColor::Green);
            break;
        }
    }
}
