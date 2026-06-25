#include "TestSpaceShipController.h"

#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/health/ShipHealthComponent.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/ui/ship_hud/ShipHudWidget.h>
#include <Sandbox/utilities/enums.h>
#include <Sandbox/utilities/world.h>

#include <SandboxCore/uobject_utils.h>

#include <Blueprint/WidgetLayoutLibrary.h>
#include <DrawDebugHelpers.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputActionValue.h>
#include <InputMappingContext.h>

#include <Sandbox/utilities/macros/null_checks.hpp>

ATestSpaceShipController::ATestSpaceShipController() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Input
void ATestSpaceShipController::SetupInputComponent() {
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

// Life cycle
void ATestSpaceShipController::BeginPlay() {
    Super::BeginPlay();
    initialise_hud();

    auto* world{GetWorld()};
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(world),
    });

    mission_manager = ml::get_first_actor<ATestMissionManager>(*world);

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(mission_manager),
    });

    on_mission_ended_handle =
        mission_manager->on_mission_ended.AddUObject(this, &ThisClass::on_mission_ended);

    if (mission_manager->is_ready()) {
        on_mission_manager_ready(*mission_manager);
    } else {
        on_mission_manager_ready_handle =
            mission_manager->on_ready.AddUObject(this, &ThisClass::on_mission_manager_ready);
    }
}
void ATestSpaceShipController::Tick(float dt) {
    Super::Tick(dt);

    log_config.tick(dt);

    TRY_INIT_PTR(ss, Cast<Pawn>(GetPawn()));
    update_crosshair_positions(*ss);
    update_lock_on_widget(*ss);

    if (mission_manager->mission_running()) {
        hud_widget->set_stopwatch_time(mission_manager->get_mission_stopwatch());
    }

    log_config.on_tick_end();
}

void ATestSpaceShipController::EndPlay(EEndPlayReason::Type const reason) {
    mission_manager->on_mission_ended.Remove(on_mission_ended_handle);

    if (on_mission_manager_ready_handle.IsValid()) {
        mission_manager->on_ready.Remove(on_mission_manager_ready_handle);
    }

    if (on_mission_manager_ready_handle.IsValid()) {
        mission_manager->on_mission_update.Remove(on_mission_update_handle);
    }

    Super::EndPlay(reason);
}

// Pawn possession
void ATestSpaceShipController::OnPossess(APawn* in_pawn) {
    Super::OnPossess(in_pawn);

    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));
    subsystem->AddMappingContext(input.base_context, 0);

    initialise_hud();

    RETURN_IF_NULLPTR(hud_widget);
    TRY_INIT_PTR(ship, Cast<Pawn>(in_pawn));

    auto& health_delegate{ship->on_health_changed};
    health_delegate.BindUObject(hud_widget, &UShipHudWidget::set_health);
    hud_widget->set_health(ship->get_health_info());
    ship->on_speed_changed.BindUObject(hud_widget, &UShipHudWidget::set_speed);
    hud_widget->set_speed(ship->get_speed());
    ship->on_energy_changed.BindUObject(hud_widget, &UShipHudWidget::set_energy);
    hud_widget->set_energy(1.f);
    ship->on_bombs_changed.BindUObject(hud_widget, &UShipHudWidget::set_bombs);
    hud_widget->set_bombs(ship->get_bombs());
    ship->on_laser_mode_changed.BindUObject(this, &ThisClass::on_laser_firing_mode_changed);
    on_laser_firing_mode_changed(ELaserFiringMode::idle);
    ship->on_lock_on_acquired.BindUObject(this, &ThisClass::on_lock_on_acquired);
    on_lock_on_acquired(nullptr);

#if WITH_EDITOR
    ship->on_speed_sampled.BindUObject(hud_widget, &UShipHudWidget::update_sampled_speed);
#endif
}
void ATestSpaceShipController::OnUnPossess() {
    if (auto* ship{Cast<Pawn>(GetPawn())}) {
        ship->SetActorTickEnabled(false);

        ship->on_health_changed.Unbind();
        ship->on_speed_changed.Unbind();
        ship->on_energy_changed.Unbind();
        ship->on_bombs_changed.Unbind();
        ship->on_laser_mode_changed.Unbind();
        ship->on_lock_on_acquired.Unbind();

#if WITH_EDITOR
        ship->on_speed_sampled.Unbind();
#endif
    }

    Super::OnUnPossess();
}

// UI
void ATestSpaceShipController::initialise_hud() {
    if (hud_widget) { return; }

    ml::fatal_if_uobject_ptrs_invalid({SANDBOX_NAMED_UOBJECT_PTR(hud_widget_class)});

    hud_widget = CreateWidget<UShipHudWidget>(this, hud_widget_class, TEXT("ship_hud"));
    ml::fatal_if_uobject_ptrs_invalid({SANDBOX_NAMED_UOBJECT_PTR(hud_widget)});
    hud_widget->AddToViewport();

    hud_widget->set_gold_rings_widget_visibility(ESlateVisibility::Collapsed);
    hud_widget->set_lives_widget_visibility(ESlateVisibility::Collapsed);
    hud_widget->set_bombs_widget_visibility(ESlateVisibility::Collapsed);

    hud_widget->set_points(0);

    hud_widget->set_stopwatch_time(0.f);
}
void ATestSpaceShipController::update_crosshair_positions(ATestSpaceShip const& ship) {
    RETURN_IF_NULLPTR(hud_widget);

    auto const ship_socket{ship.get_middle_socket()};

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

        if (log_config.can_tick_log(EActorLogVerbosity::Verbose)) {
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
#endif
}
void ATestSpaceShipController::update_lock_on_widget(ATestSpaceShip const& ship) {
    auto const* tgt{ship.get_lock_on_target()};
    if (!tgt) { return; }
    RETURN_IF_NULLPTR(hud_widget);

    auto const actor_pos{tgt->GetActorLocation()};
    FVector2d screen_pos{};

    constexpr bool bPlayerViewportRelative{false};
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            this, actor_pos, screen_pos, bPlayerViewportRelative)) {
        UE_LOG(LogSandboxController, Warning, TEXT("Failed to project actor position."));
    }

    hud_widget->set_lock_on_widget_position(screen_pos);
}

// Movement
void ATestSpaceShipController::turn(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->turn(value.Get<FVector2D>());
}
void ATestSpaceShipController::turn_completed(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->turn(FVector2D::ZeroVector);
}
void ATestSpaceShipController::start_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("Begin roll: %.1f"), value.Get<float>());
}
void ATestSpaceShipController::roll(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));

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
void ATestSpaceShipController::stop_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("End roll: %.1f"), value.Get<float>());
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->roll(0.f);
    barrel_roll_input.threshold_crossed_this_input = false;
}
void ATestSpaceShipController::barrel_roll(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->barrel_roll(value.Get<float>());
}

// Boost / brake
void ATestSpaceShipController::start_boost(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->start_boost();
}
void ATestSpaceShipController::stop_boost(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->stop_boost();
}
void ATestSpaceShipController::start_brake(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->start_brake();
}
void ATestSpaceShipController::stop_brake(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->stop_brake();
}

// Laser
void ATestSpaceShipController::start_fire_laser() {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->start_fire_laser();
}
void ATestSpaceShipController::stop_fire_laser() {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->stop_fire_laser();
}

void ATestSpaceShipController::on_laser_firing_mode_changed(ELaserFiringMode mode) {
    RETURN_IF_NULLPTR(hud_widget);

    switch (mode) {
        case ELaserFiringMode::lock_on_searching: {
            hud_widget->set_crosshair_colours(FLinearColor::Yellow, FLinearColor::Red);
            break;
        }
        case ELaserFiringMode::lock_on_acquired: {
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
void ATestSpaceShipController::on_lock_on_acquired(AActor* target) {
    RETURN_IF_NULLPTR(hud_widget);
    hud_widget->set_lock_on_widget_visibility(target != nullptr);
}

// Bomb
void ATestSpaceShipController::fire_bomb(FInputActionValue const& value) {
    TRY_INIT_PTR(ship, Cast<Pawn>(GetPawn()));
    ship->fire_bomb();
}

// Mission
void ATestSpaceShipController::on_mission_manager_ready(ATestMissionManager const& manager) {
    check(&manager == mission_manager.Get());

    initialise_from_mission_manager(manager);
}
void ATestSpaceShipController::initialise_from_mission_manager(ATestMissionManager const& manager) {
    FString const mission_status{make_mission_status_message(manager)};
    hud_widget->set_mission_status(mission_status);

    on_mission_update_handle =
        mission_manager->on_mission_update.AddUObject(this, &ThisClass::on_mission_update);
}
void ATestSpaceShipController::on_mission_update(ATestMissionManager const& manager) {
    FString const mission_status{make_mission_status_message(manager)};
    hud_widget->set_mission_status(mission_status);
    hud_widget->set_points(manager.get_player_kills());
}
void ATestSpaceShipController::on_mission_ended(ATestMissionManager const& manager) {
    check(&manager == mission_manager.Get());

    FString const mission_status{make_mission_status_message(manager)};
    hud_widget->set_mission_status(mission_status);
}
auto ATestSpaceShipController::make_mission_status_message(ATestMissionManager const& manager) const
    -> FString {
    auto const mission_mode{manager.get_mission_mode()};
    auto const mission_state{manager.get_mission_state()};

    FString status_msg;

    switch (mission_mode) {
        case ETestMissionMode::None: {
            status_msg = TEXT("No mission running");
            break;
        }
        case ETestMissionMode::SurviveTime: {
            status_msg = FString::Printf(TEXT("Survive for %d seconds"),
                                         static_cast<int32>(manager.get_survive_seconds()));
            break;
        }
        case ETestMissionMode::KillEnemies: {
            status_msg = FString::Printf(TEXT("Kill enemies (%d / %d)"),
                                         manager.get_player_kills(),
                                         manager.get_kill_target());
            break;
        }
        case ETestMissionMode::KillEnemiesWithinTime: {
            auto const kill_target{manager.get_kill_target()};
            status_msg = FString::Printf(TEXT("Kill %d enemies within %.2f seconds (%d / %d)"),
                                         kill_target,
                                         manager.get_target_time(),
                                         manager.get_player_kills(),
                                         kill_target);
            break;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }

    status_msg += FString::Printf(TEXT(" (%s)"), *ml::to_string_without_type_prefix(mission_state));

    return status_msg;
}
