#include <SpaceGame/ships/player/SpaceGamePlayerController.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/presentation/widgets/ShipHudWidget.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/ui/common/GameUiRootLayout.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>

#include <Camera/CameraActor.h>
#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <Engine/GameViewportClient.h>
#include <Engine/LocalPlayer.h>
#include <EngineUtils.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <Kismet/KismetSystemLibrary.h>
#include <TimerManager.h>
#include <UnrealClient.h>

#include <SandboxGameShared/utilities/macros/null_checks.hpp>

namespace {
constexpr int32 global_mapping_priority{100};
}

ASpaceGamePlayerController::ASpaceGamePlayerController() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
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
        case EPlayerControlContext::Ship: {
            return ship_control_context_.can_bind();
        }
    }
    return false;
}

auto ASpaceGamePlayerController::bind_context(EPlayerControlContext const context) -> bool {
    switch (context) {
        case EPlayerControlContext::None: {
            return true;
        }
        case EPlayerControlContext::Ship: {
            return ship_control_context_.bind();
        }
    }
    return false;
}

void ASpaceGamePlayerController::unbind_context(EPlayerControlContext const context) {
    switch (context) {
        case EPlayerControlContext::None: {
            break;
        }
        case EPlayerControlContext::Ship: {
            ship_control_context_.unbind();
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
            if (return_to_level_select_pending_ ||
                (IsValid(completion_menu) && completion_menu->IsActivated())) {
                return;
            }
            if (!IsValid(ui_root) || !IsValid(global_input.toggle_menu)) {
                UE_LOG(LogSandboxController,
                       Error,
                       TEXT("ASpaceGamePlayerController::toggle_pause_game: Pause UI is not "
                            "available."));
                return;
            }

            if (!suspend_gameplay_for_modal()) {
                return;
            }

            pause_menu = ui_root->show_pause_menu(*global_input.toggle_menu);
            if (!IsValid(pause_menu)) {
                resume_game();
                return;
            }
            pause_menu->return_to_level_select_requested.RemoveAll(this);
            pause_menu->return_to_level_select_requested.AddUObject(
                this, &ThisClass::return_to_level_select);
            pause_menu->quit_requested.RemoveAll(this);
            pause_menu->quit_requested.AddUObject(this, &ThisClass::quit_game);
            pause_menu->OnDeactivated().RemoveAll(this);
            pause_menu->OnDeactivated().AddUObject(this, &ThisClass::on_pause_menu_deactivated);
            break;
        }
        case EOrchestratorState::Uninitialised:
        case EOrchestratorState::Paused: {
            if (IsValid(pause_menu) && pause_menu->IsActivated()) {
                pause_menu->DeactivateWidget();
            } else if (!IsValid(completion_menu)) {
                resume_game();
            }
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
    initialise_ui_root();
    if (main_menu_requested_) {
        initialise_main_menu();
        return;
    }

    initialise_gameplay();
}

void ASpaceGamePlayerController::initialise_gameplay() {
    FInputModeGameOnly input_mode{};
    SetInputMode(input_mode);
    SetShowMouseCursor(false);

    bind_orchestrator_events();
    initialise_hud();

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
    shutting_down_ui_ = true;
    detach_modal_callbacks();
    pause_menu = nullptr;
    completion_menu = nullptr;
    shutdown_ui_root();
    set_control_context(EPlayerControlContext::None);
    ship_control_context_.shutdown();
    shutdown_global_input();

    if (auto* const orchestrator{hud_orchestrator.Get()}; IsValid(orchestrator)) {
        orchestrator->on_reset.RemoveAll(this);
        orchestrator->on_mission_completed.RemoveAll(this);
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
    if (begin_play_finished_ && !IsValid(pause_menu) && !main_menu_requested_) {
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

    if (!IsValid(ui_root) && !initialise_ui_root()) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    auto level_select_request{IsValid(subsystem) ? subsystem->take_level_select_request()
                                                 : TOptional<ml::ioj::FLevelSelectRequest>{}};
    auto const show_level_select{IsValid(subsystem) && (level_select_request.IsSet() ||
                                                        subsystem->has_level_launch_error())};
    auto const preferred_level_id{
        level_select_request.IsSet() ? level_select_request->preferred_level_id : NAME_None};
    if (!ui_root->show_main_menu(show_level_select, preferred_level_id)) {
        return;
    }

    apply_main_menu_input_mode();
    GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::apply_main_menu_input_mode);
    select_main_menu_camera();
    SetActorTickEnabled(false);
}

void ASpaceGamePlayerController::apply_main_menu_input_mode() {
    if (!main_menu_requested_ || !IsValid(ui_root) || !IsValid(ui_root->get_active_screen())) {
        return;
    }

    FInputModeUIOnly input_mode{};
    input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(input_mode);
    SetShowMouseCursor(true);
}

auto ASpaceGamePlayerController::initialise_ui_root() -> bool {
    if (IsValid(ui_root)) {
        return true;
    }
    if (!IsValid(ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_ui_root: UI data is invalid."));
        return false;
    }

    auto const root_class{ui_data->get_widget_class<ml::ioj::UGameUiRootLayout>()};
    if (!root_class) {
        return false;
    }
    auto* const root{
        CreateWidget<ml::ioj::UGameUiRootLayout>(this, root_class, TEXT("game_ui_root"))};
    if (!IsValid(root) || !root->initialise(*ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_ui_root: Failed to create root."));
        return false;
    }

    ui_root = root;
    root->AddToPlayerScreen(100);
    root->ActivateWidget();
    return true;
}

void ASpaceGamePlayerController::shutdown_ui_root() {
    if (!IsValid(ui_root)) {
        return;
    }
    ui_root->clear_menus();
    ui_root->DeactivateWidget();
    ui_root->RemoveFromParent();
    ui_root = nullptr;
}

void ASpaceGamePlayerController::select_main_menu_camera() {
    static FName const camera_tag{TEXT("MainMenuCamera")};
    for (TActorIterator<ACameraActor> it{GetWorld()}; it; ++it) {
        if (it->ActorHasTag(camera_tag)) {
            SetViewTarget(*it);
            return;
        }
    }

    UE_LOG(LogSandboxController,
           Warning,
           TEXT("ASpaceGamePlayerController::select_main_menu_camera: No camera tagged '%s' was "
                "found."),
           *camera_tag.ToString());
}

void ASpaceGamePlayerController::bind_orchestrator_events() {
    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::bind_orchestrator_events: World is invalid."));
        return;
    }

    auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(
            LogSandboxController,
            Error,
            TEXT("ASpaceGamePlayerController::bind_orchestrator_events: Orchestrator is invalid."));
        return;
    }

    hud_orchestrator = orchestrator;
    orchestrator->on_reset.RemoveAll(this);
    orchestrator->on_reset.AddUObject(this, &ThisClass::on_orchestrator_reset);
    orchestrator->on_mission_completed.RemoveAll(this);
    orchestrator->on_mission_completed.AddUObject(this, &ThisClass::on_mission_completed);
}

void ASpaceGamePlayerController::on_orchestrator_reset(ATestBatchOrchestrator& orchestrator) {
    detach_modal_callbacks();
    if (IsValid(pause_menu)) {
        pause_menu->DeactivateWidget();
    }
    if (IsValid(completion_menu)) {
        completion_menu->DeactivateWidget();
    }
    restore_hud_after_modal();
    pause_menu = nullptr;
    completion_menu = nullptr;
    restore_ship_controls_after_modal_ = false;
    modal_resume_pending_ = false;
    return_to_level_select_pending_ = false;
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

void ASpaceGamePlayerController::hide_hud_for_modal() {
    if (hud_restore_pending_ || !IsValid(hud_widget)) {
        return;
    }

    hud_visibility_before_modal_ = hud_widget->GetVisibility();
    hud_restore_pending_ = true;
    hud_widget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASpaceGamePlayerController::restore_hud_after_modal() {
    if (!hud_restore_pending_) {
        return;
    }

    if (IsValid(hud_widget)) {
        hud_widget->SetVisibility(hud_visibility_before_modal_);
    }
    hud_restore_pending_ = false;
}

auto ASpaceGamePlayerController::suspend_gameplay_for_modal() -> bool {
    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator) || orchestrator->get_state() != EOrchestratorState::Running) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::suspend_gameplay_for_modal: Gameplay is not "
                    "running."));
        return false;
    }

    restore_ship_controls_after_modal_ = active_control_context_ == EPlayerControlContext::Ship;
    modal_resume_pending_ = true;
    orchestrator->pause_simulation();
    if (!set_control_context(EPlayerControlContext::None)) {
        restore_ship_controls_after_modal_ = false;
        modal_resume_pending_ = false;
        orchestrator->start_simulation();
        return false;
    }
    hide_hud_for_modal();
    return true;
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

    auto const should_restore_ship{modal_resume_pending_ ? restore_ship_controls_after_modal_
                                                         : IsValid(Cast<Pawn>(GetPawn()))};
    auto const target_context{should_restore_ship && IsValid(Cast<Pawn>(GetPawn()))
                                  ? EPlayerControlContext::Ship
                                  : EPlayerControlContext::None};
    if (!set_control_context(target_context)) {
        return;
    }
    restore_hud_after_modal();
    restore_ship_controls_after_modal_ = false;
    modal_resume_pending_ = false;

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

void ASpaceGamePlayerController::on_pause_menu_deactivated() {
    if (IsValid(pause_menu)) {
        pause_menu->OnDeactivated().RemoveAll(this);
        pause_menu->return_to_level_select_requested.RemoveAll(this);
        pause_menu->quit_requested.RemoveAll(this);
    }
    pause_menu = nullptr;
    if (!shutting_down_ui_ && !return_to_level_select_pending_) {
        resume_game();
    }
}

void ASpaceGamePlayerController::on_completion_menu_deactivated() {
    if (IsValid(completion_menu)) {
        completion_menu->OnDeactivated().RemoveAll(this);
        completion_menu->return_to_level_select_requested.RemoveAll(this);
    }
    completion_menu = nullptr;
    if (!shutting_down_ui_ && !return_to_level_select_pending_) {
        resume_game();
    }
}

void ASpaceGamePlayerController::on_mission_completed(FTestMissionCompletion const& completion) {
    if (!completion.persisted || return_to_level_select_pending_ || IsValid(pause_menu) ||
        IsValid(completion_menu)) {
        return;
    }
    if (!IsValid(ui_root) || !suspend_gameplay_for_modal()) {
        return;
    }

    completion_menu = ui_root->show_level_completion(completion.level_display_name);
    if (!IsValid(completion_menu)) {
        resume_game();
        return;
    }
    completion_menu->return_to_level_select_requested.RemoveAll(this);
    completion_menu->return_to_level_select_requested.AddUObject(
        this, &ThisClass::return_to_level_select);
    completion_menu->OnDeactivated().RemoveAll(this);
    completion_menu->OnDeactivated().AddUObject(this, &ThisClass::on_completion_menu_deactivated);
}

void ASpaceGamePlayerController::return_to_level_select() {
    if (return_to_level_select_pending_) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(subsystem) || !IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::return_to_level_select: Required gameplay "
                    "objects are invalid."));
        return;
    }

    return_to_level_select_pending_ = true;
    auto const preferred_level_id{orchestrator->get_mission_manager().get_level_id()};
    if (!subsystem->return_to_level_select(preferred_level_id)) {
        return_to_level_select_pending_ = false;
        return;
    }

    detach_modal_callbacks();
    if (IsValid(ui_root)) {
        ui_root->clear_menus();
    }
    pause_menu = nullptr;
    completion_menu = nullptr;
    hud_restore_pending_ = false;
    set_control_context(EPlayerControlContext::None);
}

void ASpaceGamePlayerController::quit_game() {
    if (return_to_level_select_pending_) {
        return;
    }
    return_to_level_select_pending_ = true;
    detach_modal_callbacks();
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ASpaceGamePlayerController::detach_modal_callbacks() {
    if (IsValid(pause_menu)) {
        pause_menu->OnDeactivated().RemoveAll(this);
        pause_menu->return_to_level_select_requested.RemoveAll(this);
        pause_menu->quit_requested.RemoveAll(this);
    }
    if (IsValid(completion_menu)) {
        completion_menu->OnDeactivated().RemoveAll(this);
        completion_menu->return_to_level_select_requested.RemoveAll(this);
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
