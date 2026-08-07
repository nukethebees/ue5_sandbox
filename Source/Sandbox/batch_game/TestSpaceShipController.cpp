#include "TestSpaceShipController.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchGameUiData.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/health/ShipHealthComponent.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/ui/ship_hud/ShipHudWidget.h>
#include <Sandbox/utilities/enums.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <SandboxCoreEngine/uobject_utils.h>

#include <Blueprint/WidgetLayoutLibrary.h>
#include <DrawDebugHelpers.h>
#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputActionValue.h>
#include <InputMappingContext.h>
#include <UnrealClient.h>

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
    bind(input.move, Triggered, &ThisClass::set_move_input);
    bind(input.move, Completed, &ThisClass::move_completed);

    bind(lateral_move_input, Triggered, &ThisClass::set_lateral_move_input);
    bind(lateral_move_input, Completed, &ThisClass::lateral_move_completed);
    bind(vertical_move_input, Triggered, &ThisClass::set_vertical_move_input);
    bind(vertical_move_input, Completed, &ThisClass::vertical_move_completed);

    bind(ship_2d_control, Started, &ThisClass::set_ship_2d_control_started);
    bind(ship_2d_control, Triggered, &ThisClass::set_ship_2d_control);
    bind(ship_2d_control, Completed, &ThisClass::ship_2d_control_completed);

    bind(ship_1d_control_x, Started, &ThisClass::set_ship_2d_control_started);
    bind(ship_1d_control_x, Triggered, &ThisClass::set_ship_1d_control_x);
    bind(ship_1d_control_x, Completed, &ThisClass::ship_2d_control_completed);
    bind(ship_1d_control_y, Started, &ThisClass::set_ship_2d_control_started);
    bind(ship_1d_control_y, Triggered, &ThisClass::set_ship_1d_control_y);
    bind(ship_1d_control_y, Completed, &ThisClass::ship_2d_control_completed);

    bind(cycle_next_control_mode_input, Started, &ThisClass::cycle_next_control_mode);
    bind(cycle_previous_control_mode_input, Started, &ThisClass::cycle_previous_control_mode);

    bind(sample_and_hold_input, Started, &ThisClass::start_sampling);
    bind(sample_and_hold_input, Completed, &ThisClass::stop_sampling);

    bind(input.turn, Triggered, &ThisClass::turn);
    bind(input.turn, Completed, &ThisClass::turn_completed);
    bind(input.roll, Started, &ThisClass::start_roll);
    bind(input.roll, Triggered, &ThisClass::roll);
    bind(input.roll, Completed, &ThisClass::stop_roll);
    bind(input.boost, Started, &ThisClass::start_boost);
    bind(input.boost, Completed, &ThisClass::stop_boost);
    bind(input.brake, Started, &ThisClass::start_brake);
    bind(input.brake, Completed, &ThisClass::stop_brake);

    bind(input.fire_laser, Started, &ThisClass::start_fire_laser);
    bind(input.fire_laser, Completed, &ThisClass::stop_fire_laser);
    bind(input.fire_bomb, Started, &ThisClass::fire_bomb);

    bind(input.cycle_prev_fire_rate, Started, &ThisClass::cycle_prev_fire_rate);
    bind(input.cycle_next_fire_rate, Started, &ThisClass::cycle_next_fire_rate);
    bind(input.cycle_input_mapping_context, Started, &ThisClass::cycle_input_mapping_context);
}
void ATestSpaceShipController::set_mapping_context(UInputMappingContext const* context) {
    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));
    check(IsValid(context));
    subsystem->AddMappingContext(context, 0);

    auto const context_name{GetNameSafe(context)};

    UE_LOG(LogSandbox, Display, TEXT("Setting context to: %s"), *context_name);
    hud_widget->set_selected_imc(context_name);
}

// Life cycle
void ATestSpaceShipController::BeginPlay() {
    Super::BeginPlay();

    auto* world{GetWorld()};
    auto* local_player{GetLocalPlayer()};
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(world),
        SANDBOX_NAMED_UOBJECT_PTR(local_player),
    });

    initialise_hud();

    mission_manager = ml::get_first_actor<ATestMissionManager>(*world);

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(mission_manager),
    });

    entity_registry = mission_manager->get_entity_registry();
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    update_entity_count_table();

    on_mission_ended_handle =
        mission_manager->on_mission_ended.AddUObject(this, &ThisClass::on_mission_ended);

    if (mission_manager->is_ready()) {
        on_mission_manager_ready(*mission_manager);
    } else {
        on_mission_manager_ready_handle =
            mission_manager->on_ready.AddUObject(this, &ThisClass::on_mission_manager_ready);
    }

    if (!IsValid(GetPawn())) {
        UE_LOG(LogSandbox,
               Display,
               TEXT("ATestSpaceShipController::BeginPlay: No valid pawn, disabling tick."));
        SetActorTickEnabled(false);
    }
}
void ATestSpaceShipController::Tick(float dt) {
    Super::Tick(dt);

    log_config.tick(dt);

    screenshot_tick(dt);

    TRY_INIT_PTR(ss, Cast<Pawn>(GetPawn()));
    update_crosshair_positions(*ss);
    update_lock_on_widget(*ss);
    update_input_widgets(*ss);

    ui_timers.tick(dt);
    if ((ui_timers.Num() > FTestSpaceShipControllerUiTimerIndices::entity_count) &&
        ui_timers.try_consume(FTestSpaceShipControllerUiTimerIndices::entity_count)) {
        update_entity_count_table();
    }

    if ((ui_timers.Num() > FTestSpaceShipControllerUiTimerIndices::mission_status) &&
        ui_timers.try_consume(FTestSpaceShipControllerUiTimerIndices::mission_status)) {
        update_mission_status_widget();
    }

    auto const new_kills{ss->get_kills()};
    if (new_kills != ui_cache.player_kills) {
        ui_cache.player_kills = new_kills;
        hud_widget->set_points(new_kills);
    }

    log_config.on_tick_end();
}

void ATestSpaceShipController::EndPlay(EEndPlayReason::Type const reason) {
    mission_manager->on_mission_ended.Remove(on_mission_ended_handle);

    if (on_mission_manager_ready_handle.IsValid()) {
        mission_manager->on_ready.Remove(on_mission_manager_ready_handle);
    }

    if (on_mission_update_handle.IsValid()) {
        mission_manager->on_mission_update.Remove(on_mission_update_handle);
    }

    Super::EndPlay(reason);
}

// Pawn possession
void ATestSpaceShipController::OnPossess(APawn* in_pawn) {
    Super::OnPossess(in_pawn);

    initialise_hud();

    RETURN_IF_NULLPTR(hud_widget);
    auto* ship{Cast<Pawn>(in_pawn)};

    if (!IsValid(ship)) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestSpaceShipController::OnPossess: Invalid pawn."));
    }

    ship->on_health_changed.BindUObject(this, &ThisClass::on_health_changed);
    on_health_changed(ship->get_health_info());

    ship->on_speed_changed.BindUObject(this, &ThisClass::on_speed_changed);
    on_speed_changed(ship->get_speed());

    ship->on_target_speed_changed.BindUObject(this, &ThisClass::on_target_speed_changed);
    on_target_speed_changed(ship->get_target_speed());

    ship->on_energy_changed.BindUObject(this, &ThisClass::on_energy_changed);
    on_energy_changed(1.f);

    ship->on_bombs_changed.BindUObject(this, &ThisClass::on_bombs_changed);
    on_bombs_changed(ship->get_bombs());

    ship->on_laser_mode_changed.BindUObject(this, &ThisClass::on_laser_firing_mode_changed);
    on_laser_firing_mode_changed(ELaserFiringState::idle);

    ship->on_lock_on_acquired.BindUObject(this, &ThisClass::on_lock_on_acquired);
    on_lock_on_acquired(nullptr);

    ship->on_ship_fire_rate_changed.BindUObject(this, &ThisClass::on_ship_fire_rate_changed);
    on_ship_fire_rate_changed(ship->get_laser_fire_rate());

    ship->on_player_ship_died.BindUObject(this, &ThisClass::on_player_ship_died);

    auto const n_contexts{input.mapping_contexts.Num()};

    check(n_contexts > 0);
    check(input_mapping_context_index >= 0);
    check(input_mapping_context_index < n_contexts);

    set_mapping_context(input.mapping_contexts[input_mapping_context_index]);

#if WITH_EDITOR
    ship->on_speed_sampled.BindUObject(this, &ThisClass::on_speed_sampled);
#endif

    hud_widget->set_crosshair_widget_visibility(ESlateVisibility::Visible);
    hud_widget->set_lock_on_widget_visibility(false);
    SetActorTickEnabled(true);
}
void ATestSpaceShipController::OnUnPossess() {
    if (auto* ship{Cast<Pawn>(GetPawn())}) {
        ship->on_health_changed.Unbind();
        ship->on_speed_changed.Unbind();
        ship->on_target_speed_changed.Unbind();
        ship->on_energy_changed.Unbind();
        ship->on_bombs_changed.Unbind();
        ship->on_laser_mode_changed.Unbind();
        ship->on_lock_on_acquired.Unbind();
        ship->on_ship_fire_rate_changed.Unbind();
        ship->on_player_ship_died.Unbind();

#if WITH_EDITOR
        ship->on_speed_sampled.Unbind();
#endif
    }

    if (auto* local_player{GetLocalPlayer()}) {
        if (auto* subsystem{
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)}) {
            if (input.mapping_contexts.IsValidIndex(input_mapping_context_index)) {
                subsystem->RemoveMappingContext(
                    input.mapping_contexts[input_mapping_context_index]);
            }
        }
    }

    if (IsValid(hud_widget)) {
        hud_widget->set_crosshair_widget_visibility(ESlateVisibility::Collapsed);
        hud_widget->set_lock_on_widget_visibility(false);
    }

    SetActorTickEnabled(false);

    Super::OnUnPossess();
}

/* ---------------------------------------------------------------------------------------------- */
// UI
/* ---------------------------------------------------------------------------------------------- */
void ATestSpaceShipController::initialise_hud() {
    if (IsValid(hud_widget)) {
        return;
    }

    ml::fatal_if_uobject_ptrs_invalid({SANDBOX_NAMED_UOBJECT_PTR(hud_widget_class)});

    hud_widget = CreateWidget<UShipHudWidget>(this, hud_widget_class, TEXT("ship_hud"));
    ml::fatal_if_uobject_ptrs_invalid({SANDBOX_NAMED_UOBJECT_PTR(hud_widget)});
    hud_widget->AddToViewport();

    hud_widget->set_gold_rings_widget_visibility(ESlateVisibility::Collapsed);
    hud_widget->set_lives_widget_visibility(ESlateVisibility::Collapsed);
    hud_widget->set_bombs_widget_visibility(ESlateVisibility::Collapsed);

    hud_widget->set_points(0);

    hud_widget->set_stopwatch_time(0.f);

    ui_timers.Reset();
    ml::report_invalid_uobject_ptrs({SANDBOX_NAMED_UOBJECT_PTR(ui_data)}, error_msg);
    if (error_msg) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ATestSpaceShipController::initialise_hud: %s"),
               *error_msg.message);
        return;
    }

    ml::report_invalid_uobject_ptrs({SANDBOX_NAMED_UOBJECT_PTR(ui_data->team_visual_data)},
                                    error_msg);
    if (error_msg) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ATestSpaceShipController::initialise_hud: %s"),
               *error_msg.message);
    } else {
        hud_widget->set_entity_colours(ui_data->team_visual_data->build_team_colour_cache());
    }

    auto const entity_count_period{ui_data->update_frequencies.entity_count_update_period};
    auto const mission_status_period{ui_data->update_frequencies.mission_status_update_period};
    if (entity_count_period <= 0.f || mission_status_period <= 0.f) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ATestSpaceShipController::initialise_hud: UI update periods must be "
                    "positive."));
        return;
    }

    ui_timers.add_started(entity_count_period);
    ui_timers.add_started(mission_status_period);
}

void ATestSpaceShipController::update_mission_status_widget() {
    if (!mission_manager || !hud_widget) {
        return;
    }

    hud_widget->set_mission_status(*mission_manager);
    hud_widget->set_stopwatch_time(mission_manager->get_mission_stopwatch());
}

void ATestSpaceShipController::update_entity_count_table() {
    if (ml::report_invalid_uobject_ptrs(
            {
                {
                    SANDBOX_NAMED_UOBJECT_PTR(hud_widget),
                    SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
                    SANDBOX_NAMED_UOBJECT_PTR(ui_data),
                },
                {SANDBOX_NAMED_UOBJECT_PTR(ui_data->team_visual_data)},
            },
            error_msg)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ATestSpaceShipController::update_entity_count_table: %s"),
               *error_msg.message);
        return;
    }

    hud_widget->set_entity_counts(entity_registry->count_alive_per_team_and_type());
}

void ATestSpaceShipController::on_health_changed(FShipHealth const value) {
    check(hud_widget);
    hud_widget->set_health(value);
}
void ATestSpaceShipController::on_speed_changed(float const value) {
    check(hud_widget);
    hud_widget->set_speed(value);
}
void ATestSpaceShipController::on_target_speed_changed(float const value) {
    check(hud_widget);
    hud_widget->set_target_speed(value);
}
void ATestSpaceShipController::on_energy_changed(float const value) {
    check(hud_widget);
    hud_widget->set_energy(value);
}
void ATestSpaceShipController::on_bombs_changed(int32 const value) {
    check(hud_widget);
    hud_widget->set_bombs(value);
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
    if (!tgt) {
        return;
    }
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

void ATestSpaceShipController::update_input_widgets(ATestSpaceShip const& ship) {
    RETURN_IF_NULLPTR(hud_widget);

    hud_widget->set_turning(ship.get_turn_input());
    hud_widget->set_moving(ship.get_move_input());
    hud_widget->set_desired_velocity_scale(ship.get_target_local_planar_velocity_scale());
    hud_widget->set_ship_velocity(ship.get_velocity());
    hud_widget->set_target_velocity(ship.get_target_local_planar_velocity());
    hud_widget->set_control_mode(*ml::to_string_without_type_prefix(ship.get_control_mode()));
    hud_widget->set_flight_mode(*ml::to_string_without_type_prefix(ship.get_flight_mode()));
}
void ATestSpaceShipController::on_ship_fire_rate_changed(ETestShipFireRate const value) {
    check(hud_widget);
    hud_widget->set_fire_rate(*ml::to_string_without_type_prefix(value));
}
void ATestSpaceShipController::on_laser_firing_mode_changed(ELaserFiringState mode) {
    check(hud_widget);

    switch (mode) {
        case ELaserFiringState::lock_on_searching: {
            hud_widget->set_crosshair_colours(FLinearColor::Yellow, FLinearColor::Red);
            break;
        }
        case ELaserFiringState::lock_on_acquired: {
            break;
        }
        default: {
            UE_LOG(LogSandboxController, Warning, TEXT("Unhandled laser firing mode."));
            [[fallthrough]];
        }
        case ELaserFiringState::idle:
            [[fallthrough]];
        case ELaserFiringState::lock_on_transition:
            [[fallthrough]];
        case ELaserFiringState::burst: {
            hud_widget->set_crosshair_colours(FLinearColor::Green, FLinearColor::Green);
            break;
        }
    }
}
void ATestSpaceShipController::on_lock_on_acquired(AActor* target) {
    check(hud_widget);
    hud_widget->set_lock_on_widget_visibility(target != nullptr);
}
void ATestSpaceShipController::on_player_ship_died() {
    UnPossess();
}

#if WITH_EDITOR
void ATestSpaceShipController::on_speed_sampled(std::span<FVector2d> const samples,
                                                int32 const oldest_index) {
    check(hud_widget);
    hud_widget->update_sampled_speed(samples, oldest_index);
}
#endif

/* ---------------------------------------------------------------------------------------------- */
// Input
/* ---------------------------------------------------------------------------------------------- */
// Movement
void ATestSpaceShipController::set_move_input(FInputActionValue const& value) {
    get_pawn().set_move_input(value.Get<FVector2D>());
}
void ATestSpaceShipController::move_completed() {
    get_pawn().set_move_input(FVector2D::ZeroVector);
}
void ATestSpaceShipController::set_lateral_move_input(FInputActionValue const& value) {
    get_pawn().set_lateral_move_input(value.Get<float>());
}
void ATestSpaceShipController::lateral_move_completed() {
    get_pawn().set_lateral_move_input(0.f);
}
void ATestSpaceShipController::set_vertical_move_input(FInputActionValue const& value) {
    get_pawn().set_vertical_move_input(value.Get<float>());
}
void ATestSpaceShipController::vertical_move_completed() {
    get_pawn().set_vertical_move_input(0.f);
}

void ATestSpaceShipController::set_ship_2d_control_started() {
    start_sampling();
}
void ATestSpaceShipController::set_ship_2d_control(FInputActionValue const& value) {
    get_pawn().set_ship_2d_control(value.Get<FVector2D>());
}
void ATestSpaceShipController::ship_2d_control_completed() {
    stop_sampling();
}
void ATestSpaceShipController::set_ship_1d_control_x(FInputActionValue const& value) {
    get_pawn().set_ship_1d_control_x(value.Get<float>());
}
void ATestSpaceShipController::set_ship_1d_control_y(FInputActionValue const& value) {
    get_pawn().set_ship_1d_control_y(value.Get<float>());
}

void ATestSpaceShipController::cycle_next_control_mode() {
    get_pawn().select_next_control_mode();
}
void ATestSpaceShipController::cycle_previous_control_mode() {
    get_pawn().select_previous_control_mode();
}

void ATestSpaceShipController::start_sampling() {
    get_pawn().start_sampling();
}
void ATestSpaceShipController::stop_sampling() {
    get_pawn().stop_sampling();
}

void ATestSpaceShipController::turn(FInputActionValue const& value) {
    get_pawn().turn(value.Get<FVector2D>());
}
void ATestSpaceShipController::turn_completed(FInputActionValue const& value) {
    get_pawn().turn(FVector2D::ZeroVector);
}
void ATestSpaceShipController::start_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("Begin roll: %.1f"), value.Get<float>());
}
void ATestSpaceShipController::roll(FInputActionValue const& value) {
    auto const direction{FMath::Clamp(value.Get<float>(), -1.f, 1.f)};
    get_pawn().roll(direction);
}
void ATestSpaceShipController::stop_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("End roll: %.1f"), value.Get<float>());
    get_pawn().roll(0.f);
}

// Boost / brake
void ATestSpaceShipController::start_boost(FInputActionValue const& value) {
    get_pawn().start_boost();
}
void ATestSpaceShipController::stop_boost(FInputActionValue const& value) {
    get_pawn().stop_boost();
}
void ATestSpaceShipController::start_brake(FInputActionValue const& value) {
    get_pawn().start_brake();
}
void ATestSpaceShipController::stop_brake(FInputActionValue const& value) {
    get_pawn().stop_brake();
}
void ATestSpaceShipController::cycle_input_mapping_context() {
    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));

    subsystem->RemoveMappingContext(input.mapping_contexts[input_mapping_context_index]);

    auto const n_contexts{input.mapping_contexts.Num()};
    input_mapping_context_index = (input_mapping_context_index + 1) % n_contexts;

    set_mapping_context(input.mapping_contexts[input_mapping_context_index]);
}

// Laser
void ATestSpaceShipController::start_fire_laser() {
    get_pawn().start_fire_laser();
}
void ATestSpaceShipController::stop_fire_laser() {
    get_pawn().stop_fire_laser();
}

void ATestSpaceShipController::cycle_prev_fire_rate() {
    get_pawn().select_previous_laser_fire_rate();
}
void ATestSpaceShipController::cycle_next_fire_rate() {
    get_pawn().select_next_laser_fire_rate();
}

// Bomb
void ATestSpaceShipController::fire_bomb(FInputActionValue const& value) {
    get_pawn().fire_bomb();
}

// Mission
void ATestSpaceShipController::on_mission_manager_ready(ATestMissionManager const& manager) {
    check(&manager == mission_manager.Get());

    initialise_from_mission_manager(manager);
}
void ATestSpaceShipController::initialise_from_mission_manager(ATestMissionManager const& manager) {
    FString const mission_status{make_mission_status_message(manager)};
    hud_widget->set_mission_status(mission_status);
    hud_widget->set_mission_status(manager);
    hud_widget->set_stopwatch_time(manager.get_mission_stopwatch());

    on_mission_update_handle =
        mission_manager->on_mission_update.AddUObject(this, &ThisClass::on_mission_update);
}
void ATestSpaceShipController::on_mission_update(ATestMissionManager const& manager) {
    FString const mission_status{make_mission_status_message(manager)};
    hud_widget->set_mission_status(mission_status);
    hud_widget->set_mission_status(manager);
    hud_widget->set_points(manager.get_mission_kills());
}
void ATestSpaceShipController::on_mission_ended(ATestMissionManager const& manager) {
    check(&manager == mission_manager.Get());

    // Do final update
    update_mission_status_widget();

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
            status_msg =
                FString::Printf(TEXT("Kill enemies (%d remaining)"), manager.get_kills_remaining());
            break;
        }
        case ETestMissionMode::KillEnemiesWithinTime: {
            status_msg = FString::Printf(TEXT("Kill %d enemies within %.2f seconds (%d remaining)"),
                                         manager.get_kill_target(),
                                         manager.get_target_time(),
                                         manager.get_kills_remaining());
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

auto ATestSpaceShipController::get_pawn() -> Pawn& {
    auto* pawn{Cast<Pawn>(GetPawn())};

    if (!IsValid(pawn)) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestSpaceShipController::get_pawn: Invalid pawn."));
    }

    return *pawn;
}

// Misc
void ATestSpaceShipController::screenshot_tick(float dt) {
    if (screenshot_period > 0.f) {
        screenshot_accumulator += dt;
        if (screenshot_accumulator >= screenshot_period) {
            screenshot_accumulator = FMath::Fmod(screenshot_accumulator, screenshot_period);

            if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) {
                GEngine->GameViewport->Viewport->TakeHighResScreenShot();
            }
        }
    }
}
