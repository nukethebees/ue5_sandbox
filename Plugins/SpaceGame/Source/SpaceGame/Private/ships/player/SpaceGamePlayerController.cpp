#include <SpaceGame/ships/player/SpaceGamePlayerController.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/presentation/widgets/ShipHudWidget.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>

#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <UnrealClient.h>
#include <UObject/ConstructorHelpers.h>

#include <SandboxGameShared/utilities/macros/null_checks.hpp>

namespace {
constexpr int32 global_mapping_priority{100};
}

ASpaceGamePlayerController::ASpaceGamePlayerController() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    static ConstructorHelpers::FClassFinder<ml::ioj::UMainMenuWidget> const widget_class{
        TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu")};
    main_menu_widget_class = widget_class.Class;
}

/* ---------------------------------------------------------------------------------------------- */
// Input orchestration
/* ---------------------------------------------------------------------------------------------- */
void ASpaceGamePlayerController::SetupInputComponent() {
    Super::SetupInputComponent();

    if (main_menu_requested_) {
        return;
    }

    TRY_INIT_PTR(input_component, Cast<UEnhancedInputComponent>(InputComponent));
    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(input_subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));

    if (!ship_control_context_.initialise(*this, *input_component, *input_subsystem, input)) {
        return;
    }
    initialise_global_input(*input_component, *input_subsystem);
}

auto ASpaceGamePlayerController::initialise_global_input(
    UEnhancedInputComponent& input_component, UEnhancedInputLocalPlayerSubsystem& input_subsystem)
    -> bool {
    if (global_input_bound_) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_global_input: Already initialised."));
        return false;
    }
    if (!IsValid(global_input.mapping_context) || !IsValid(global_input.toggle_menu)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_global_input: Input configuration is "
                    "invalid."));
        return false;
    }

    auto& binding{input_component.BindAction(
        global_input.toggle_menu, ETriggerEvent::Started, this, &ThisClass::toggle_pause_game)};
    global_input_binding_handle_ = binding.GetHandle();
    global_input_component_ = &input_component;
    global_input_subsystem_ = &input_subsystem;
    input_subsystem.AddMappingContext(global_input.mapping_context, global_mapping_priority);
    global_input_bound_ = true;
    return true;
}

void ASpaceGamePlayerController::shutdown_global_input() {
    if (!global_input_bound_) {
        return;
    }

    if (auto* const input_subsystem{global_input_subsystem_.Get()};
        IsValid(input_subsystem) && IsValid(global_input.mapping_context)) {
        input_subsystem->RemoveMappingContext(global_input.mapping_context);
    }
    if (auto* const input_component{global_input_component_.Get()}; IsValid(input_component)) {
        input_component->RemoveBindingByHandle(global_input_binding_handle_);
    }

    global_input_binding_handle_ = 0;
    global_input_subsystem_.Reset();
    global_input_component_.Reset();
    global_input_bound_ = false;
}

auto ASpaceGamePlayerController::can_bind_context(EPlayerControlContext const context) const
    -> bool {
    switch (context) {
        case EPlayerControlContext::None: {
            return true;
        }
        case EPlayerControlContext::MainMenu: {
            return main_menu_control_context_.can_bind();
        }
        case EPlayerControlContext::Ship: {
            return ship_control_context_.can_bind();
        }
        case EPlayerControlContext::PauseMenu: {
            return pause_menu_control_context_.can_bind();
        }
    }
    return false;
}

auto ASpaceGamePlayerController::bind_context(EPlayerControlContext const context) -> bool {
    switch (context) {
        case EPlayerControlContext::None: {
            return true;
        }
        case EPlayerControlContext::MainMenu: {
            return main_menu_control_context_.bind();
        }
        case EPlayerControlContext::Ship: {
            return ship_control_context_.bind();
        }
        case EPlayerControlContext::PauseMenu: {
            return pause_menu_control_context_.bind();
        }
    }
    return false;
}

void ASpaceGamePlayerController::unbind_context(EPlayerControlContext const context) {
    switch (context) {
        case EPlayerControlContext::None: {
            break;
        }
        case EPlayerControlContext::MainMenu: {
            main_menu_control_context_.unbind();
            break;
        }
        case EPlayerControlContext::Ship: {
            ship_control_context_.unbind();
            break;
        }
        case EPlayerControlContext::PauseMenu: {
            pause_menu_control_context_.unbind();
            break;
        }
    }
}

auto ASpaceGamePlayerController::set_control_context(EPlayerControlContext const context) -> bool {
    if (context == active_control_context_) {
        return bind_context(context);
    }
    if (!can_bind_context(context)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::set_control_context: Requested context cannot "
                    "bind."));
        return false;
    }

    auto const previous_context{active_control_context_};
    unbind_context(previous_context);
    active_control_context_ = EPlayerControlContext::None;

    if (bind_context(context)) {
        active_control_context_ = context;
        return true;
    }

    UE_LOG(LogSandboxController,
           Error,
           TEXT("ASpaceGamePlayerController::set_control_context: Failed to bind requested "
                "context."));
    if (bind_context(previous_context)) {
        active_control_context_ = previous_context;
    } else {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::set_control_context: Failed to restore previous "
                    "context."));
    }
    return false;
}

void ASpaceGamePlayerController::on_ship_mapping_context_changed(
    UInputMappingContext const& context) {
    auto const context_name{GetNameSafe(&context)};
    UE_LOG(LogSandbox, Display, TEXT("Setting context to: %s"), *context_name);

    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::on_ship_mapping_context_changed: HUD "
                    "orchestrator is invalid."));
        return;
    }
    orchestrator->get_hud_manager().set_selected_mapping_context(context_name);
}

void ASpaceGamePlayerController::toggle_pause_game() {
    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::toggle_pause_game: Orchestrator is invalid."));
        return;
    }

    switch (orchestrator->get_state()) {
        case EOrchestratorState::Running: {
            if (!pause_menu_control_context_.can_bind()) {
                UE_LOG(LogSandboxController,
                       Error,
                       TEXT("ASpaceGamePlayerController::toggle_pause_game: Menu context is not "
                            "available."));
                return;
            }

            orchestrator->pause_simulation();
            if (!set_control_context(EPlayerControlContext::PauseMenu)) {
                orchestrator->start_simulation();
            }
            break;
        }
        case EOrchestratorState::Uninitialised:
        case EOrchestratorState::Paused: {
            resume_game();
            break;
        }
        case EOrchestratorState::Stopped: {
            UE_LOG(LogSandboxController,
                   Error,
                   TEXT("ASpaceGamePlayerController::toggle_pause_game: Orchestrator is stopped."));
            break;
        }
    }
}

/* ---------------------------------------------------------------------------------------------- */
// Life cycle
/* ---------------------------------------------------------------------------------------------- */
void ASpaceGamePlayerController::BeginPlay() {
    Super::BeginPlay();

    begin_play_finished_ = true;
    if (main_menu_requested_) {
        initialise_main_menu();
        return;
    }

    initialise_gameplay();
}

void ASpaceGamePlayerController::initialise_gameplay() {
    bind_orchestrator_reset();
    initialise_hud();
    if (IsValid(ui_data)) {
        pause_menu_control_context_.initialise(*this, *ui_data);
    } else {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_gameplay: UI data is invalid."));
    }

    auto* const ship{Cast<Pawn>(GetPawn())};
    if (IsValid(ship)) {
        ship_control_context_.set_ship(ship);
        set_control_context(EPlayerControlContext::Ship);
    } else {
        UE_LOG(LogSandbox,
               Display,
               TEXT("ASpaceGamePlayerController::BeginPlay: No valid pawn, disabling tick."));
        SetActorTickEnabled(false);
    }
}

void ASpaceGamePlayerController::Tick(float const dt) {
    Super::Tick(dt);

    log_config.tick(dt);
    screenshot_tick(dt);
    log_config.on_tick_end();
}

void ASpaceGamePlayerController::EndPlay(EEndPlayReason::Type const reason) {
    set_control_context(EPlayerControlContext::None);
    main_menu_control_context_.shutdown();
    pause_menu_control_context_.shutdown();
    ship_control_context_.shutdown();
    shutdown_global_input();

    if (auto* const orchestrator{hud_orchestrator.Get()}; IsValid(orchestrator)) {
        orchestrator->on_reset.RemoveAll(this);
    }

    if (IsValid(hud_widget)) {
        if (auto* const orchestrator{hud_orchestrator.Get()};
            IsValid(orchestrator) &&
            orchestrator->get_hud_manager().get_state() == EHUDManagerState::Active) {
            orchestrator->get_hud_manager().unregister_hud(*hud_widget);
        }
        hud_widget->RemoveFromParent();
    }
    hud_widget = nullptr;
    hud_orchestrator.Reset();

    Super::EndPlay(reason);
}

/* ---------------------------------------------------------------------------------------------- */
// Pawn possession
/* ---------------------------------------------------------------------------------------------- */
void ASpaceGamePlayerController::OnPossess(APawn* const in_pawn) {
    Super::OnPossess(in_pawn);

    if (!main_menu_requested_) {
        initialise_hud();
    }

    auto* const ship{Cast<Pawn>(in_pawn)};
    if (!IsValid(ship)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ASpaceGamePlayerController::OnPossess: Player ship is invalid."));
        SetActorTickEnabled(false);
        return;
    }

    ship->on_player_ship_died.BindUObject(this, &ThisClass::on_player_ship_died);
    ship_control_context_.set_ship(ship);
    if (begin_play_finished_ && active_control_context_ != EPlayerControlContext::MainMenu &&
        active_control_context_ != EPlayerControlContext::PauseMenu) {
        set_control_context(EPlayerControlContext::Ship);
    }

    SetActorTickEnabled(true);
    UE_LOG(LogSandbox, Display, TEXT("Possessed player ship"));
}

void ASpaceGamePlayerController::OnUnPossess() {
    if (auto* const ship{Cast<Pawn>(GetPawn())}) {
        ship->on_player_ship_died.Unbind();
    }

    if (active_control_context_ == EPlayerControlContext::Ship) {
        set_control_context(EPlayerControlContext::None);
    }
    ship_control_context_.set_ship(nullptr);

    SetActorTickEnabled(false);
    UE_LOG(LogSandbox, Display, TEXT("Unpossessed player ship"));
    Super::OnUnPossess();
}

void ASpaceGamePlayerController::on_player_ship_died() {
    UnPossess();
}

/* ---------------------------------------------------------------------------------------------- */
// UI and simulation transitions
/* ---------------------------------------------------------------------------------------------- */
void ASpaceGamePlayerController::show_main_menu() {
    main_menu_requested_ = true;
    if (begin_play_finished_) {
        initialise_main_menu();
    }
}

void ASpaceGamePlayerController::initialise_main_menu() {
    set_control_context(EPlayerControlContext::None);
    ship_control_context_.shutdown();
    shutdown_global_input();

    if (!main_menu_control_context_.is_initialised() &&
        !main_menu_control_context_.initialise(*this, main_menu_widget_class)) {
        return;
    }

    set_control_context(EPlayerControlContext::MainMenu);
    SetActorTickEnabled(false);
}

void ASpaceGamePlayerController::bind_orchestrator_reset() {
    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::bind_orchestrator_reset: World is invalid."));
        return;
    }

    auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(
            LogSandboxController,
            Error,
            TEXT("ASpaceGamePlayerController::bind_orchestrator_reset: Orchestrator is invalid."));
        return;
    }

    hud_orchestrator = orchestrator;
    orchestrator->on_reset.RemoveAll(this);
    orchestrator->on_reset.AddUObject(this, &ThisClass::on_orchestrator_reset);
}

void ASpaceGamePlayerController::on_orchestrator_reset(ATestBatchOrchestrator& orchestrator) {
    set_control_context(EPlayerControlContext::None);

    auto* const player_ship{const_cast<ATestSpaceShip*>(orchestrator.get_player_ship())};
    if (!IsValid(player_ship)) {
        if (IsValid(GetPawn())) {
            UnPossess();
        }
        return;
    }

    if (GetPawn() != player_ship) {
        Possess(player_ship);
    } else {
        ship_control_context_.set_ship(player_ship);
        set_control_context(EPlayerControlContext::Ship);
    }
}

void ASpaceGamePlayerController::initialise_hud() {
    if (IsValid(hud_widget)) {
        return;
    }

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_hud: World is not available yet."));
        return;
    }
    if (!IsValid(GetLocalPlayer())) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_hud: Local player is not available "
                    "yet."));
        return;
    }

    auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_hud: Orchestrator is not available "
                    "yet."));
        return;
    }
    if (!IsValid(ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_hud: UI data is invalid."));
        return;
    }
    hud_orchestrator = orchestrator;

    auto* const team_visual_data{ui_data->team_visual_data.Get()};
    if (!IsValid(team_visual_data)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_hud: Team visual data is invalid."));
        return;
    }

    auto const hud_widget_class{ui_data->get_widget_class<UShipHudWidget>()};
    if (!hud_widget_class) {
        return;
    }

    auto* const created_widget{
        CreateWidget<UShipHudWidget>(this, hud_widget_class, TEXT("ship_hud"))};
    if (!IsValid(created_widget)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_hud: Failed to create HUD widget."));
        return;
    }

    hud_widget = created_widget;
    created_widget->AddToViewport();
    created_widget->set_entity_colours(team_visual_data->build_team_colour_cache());
    created_widget->set_crosshair_distances(ui_data->crosshair_distances);
    orchestrator->get_hud_manager().register_hud(*created_widget);
}

void ASpaceGamePlayerController::resume_game() {
    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::resume_game: Orchestrator is invalid."));
        return;
    }
    if (orchestrator->get_state() == EOrchestratorState::Stopped) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::resume_game: Orchestrator is stopped."));
        return;
    }

    auto const target_context{IsValid(Cast<Pawn>(GetPawn())) ? EPlayerControlContext::Ship
                                                             : EPlayerControlContext::None};
    if (!set_control_context(target_context)) {
        return;
    }

    switch (orchestrator->get_state()) {
        case EOrchestratorState::Uninitialised:
        case EOrchestratorState::Paused: {
            orchestrator->start_simulation();
            break;
        }
        case EOrchestratorState::Running:
        case EOrchestratorState::Stopped: {
            break;
        }
    }
}

/* ---------------------------------------------------------------------------------------------- */
// Misc
/* ---------------------------------------------------------------------------------------------- */
void ASpaceGamePlayerController::screenshot_tick(float const dt) {
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
