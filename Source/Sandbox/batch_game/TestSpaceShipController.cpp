#include "TestSpaceShipController.h"

#include <Sandbox/batch_game/TestBatchGameUiData.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/ui/ship_hud/ShipHudWidget.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <SandboxCoreEngine/uobject_utils.h>

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
    if (!IsValid(context)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ATestSpaceShipController::set_mapping_context: Context is invalid."));
        return;
    }
    subsystem->AddMappingContext(context, 0);

    auto const context_name{GetNameSafe(context)};

    UE_LOG(LogSandbox, Display, TEXT("Setting context to: %s"), *context_name);
    hud_manager.set_selected_mapping_context(context_name);
}

// Life cycle
void ATestSpaceShipController::BeginPlay() {
    Super::BeginPlay();

    initialise_hud();

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

    log_config.on_tick_end();
}

void ATestSpaceShipController::EndPlay(EEndPlayReason::Type const reason) {
    if (auto* const world{GetWorld()}; IsValid(world)) {
        if (auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)}) {
            orchestrator->unregister_hud_manager(hud_manager);
        }
    }
    hud_manager.deactivate();

    Super::EndPlay(reason);
}

// Pawn possession
void ATestSpaceShipController::OnPossess(APawn* in_pawn) {
    Super::OnPossess(in_pawn);

    initialise_hud();

    auto* const ship{Cast<Pawn>(in_pawn)};
    if (!IsValid(ship)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestSpaceShipController::OnPossess: Player ship is invalid."));
        SetActorTickEnabled(false);
        return;
    }

    if (hud_manager.get_state() == EHUDManagerState::Active) {
        hud_manager.set_player_ship(ship);

        if (auto* const world{GetWorld()}; IsValid(world)) {
            if (auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)}) {
                orchestrator->register_hud_manager(hud_manager);
            }
        }
    }

    ship->on_player_ship_died.BindUObject(this, &ThisClass::on_player_ship_died);

    auto const n_contexts{input.mapping_contexts.Num()};
    if (n_contexts <= 0 || !input.mapping_contexts.IsValidIndex(input_mapping_context_index)) {
        UE_LOG(
            LogSandbox,
            Error,
            TEXT("ATestSpaceShipController::OnPossess: Input mapping context index is invalid."));
    } else {
        set_mapping_context(input.mapping_contexts[input_mapping_context_index]);
    }

    SetActorTickEnabled(true);

    UE_LOG(LogSandbox, Display, TEXT("Possessed player ship"));
}
void ATestSpaceShipController::OnUnPossess() {
    if (auto* ship{Cast<Pawn>(GetPawn())}) {
        ship->on_player_ship_died.Unbind();
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

    hud_manager.clear_player_ship();

    SetActorTickEnabled(false);
    UE_LOG(LogSandbox, Display, TEXT("Unpossessed player ship"));

    Super::OnUnPossess();
}

/* ---------------------------------------------------------------------------------------------- */
// UI
/* ---------------------------------------------------------------------------------------------- */
void ATestSpaceShipController::initialise_hud() {
    if (hud_manager.get_state() == EHUDManagerState::Active && IsValid(hud_widget)) {
        return;
    }

    auto* const world{GetWorld()};
    auto* const local_player{GetLocalPlayer()};
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(world),
        SANDBOX_NAMED_UOBJECT_PTR(local_player),
    });

    auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
    auto* const mission_manager{orchestrator ? orchestrator->get_mission_manager() : nullptr};
    auto* const entity_registry{orchestrator ? orchestrator->get_entity_registry() : nullptr};
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(orchestrator),
        SANDBOX_NAMED_UOBJECT_PTR(mission_manager),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        SANDBOX_NAMED_UOBJECT_PTR(ui_data),
        SANDBOX_NAMED_UOBJECT_PTR(hud_widget_class),
    });

    auto* const team_visual_data{ui_data->team_visual_data.Get()};
    if (!IsValid(team_visual_data)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestSpaceShipController::initialise_hud: Team visual data is invalid."));
        return;
    }

    auto* const created_widget{
        CreateWidget<UShipHudWidget>(this, hud_widget_class, TEXT("ship_hud"))};
    if (!IsValid(created_widget)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ATestSpaceShipController::initialise_hud: Failed to create HUD widget."));
        return;
    }

    auto* const player_ship{Cast<Pawn>(GetPawn())};
    hud_widget = created_widget;
    hud_manager.initialise(*created_widget,
                           *ui_data,
                           *mission_manager,
                           *entity_registry,
                           {*orchestrator},
                           *this,
                           player_ship);

    if (hud_manager.get_state() == EHUDManagerState::Active) {
        orchestrator->register_hud_manager(hud_manager);
    } else {
        hud_widget = nullptr;
    }
}
void ATestSpaceShipController::on_player_ship_died() {
    UnPossess();
}

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

    auto const n_contexts{input.mapping_contexts.Num()};
    if (n_contexts <= 0 || !input.mapping_contexts.IsValidIndex(input_mapping_context_index)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ATestSpaceShipController::cycle_input_mapping_context: Input mapping "
                    "context index is invalid."));
        return;
    }

    subsystem->RemoveMappingContext(input.mapping_contexts[input_mapping_context_index]);
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
