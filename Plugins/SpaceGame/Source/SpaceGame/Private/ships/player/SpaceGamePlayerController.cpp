#include <SpaceGame/ships/player/SpaceGamePlayerController.h>

#include <ioj/sim/mission_manager.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SpaceGame/input/CanonicalShipControls.h>
#include <SpaceGame/input/SpaceGameInputUserSettings.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/settings/GameSettingsSubsystem.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Camera/CameraActor.h>
#include <CommonInputSubsystem.h>
#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <Engine/GameViewportClient.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <HAL/IConsoleManager.h>
#include <InputAction.h>
#include <InputCoreTypes.h>
#include <InputKeyEventArgs.h>
#include <InputMappingContext.h>
#include <Kismet/KismetSystemLibrary.h>
#include <TimerManager.h>
#include <UnrealClient.h>
#include <UObject/ConstructorHelpers.h>
#include <UserSettings/EnhancedInputUserSettings.h>

#include <SandboxGameShared/utilities/macros/null_checks.hpp>

namespace spacegame::input_trace {
static TAutoConsoleVariable<int32> enabled{
    TEXT("spacegame.InputTrace"),
    0,
    TEXT("Log gamepad A, pitch-stick, right-trigger, mouse-click and mouse-Y events with active "
         "mappings and ship handlers.")};
}

ASpaceGamePlayerController::ASpaceGamePlayerController() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> observer_mapping{
        TEXT("/SpaceGame/Input/Observer/IMC_Observer")};
    static ConstructorHelpers::FObjectFinder<UInputAction> observer_move{
        TEXT("/SpaceGame/Input/Observer/IA_ObserverMove")};
    static ConstructorHelpers::FObjectFinder<UInputAction> observer_vertical_move{
        TEXT("/SpaceGame/Input/Observer/IA_ObserverVerticalMove")};
    static ConstructorHelpers::FObjectFinder<UInputAction> observer_look{
        TEXT("/SpaceGame/Input/Observer/IA_ObserverLook")};
    static ConstructorHelpers::FObjectFinder<UInputAction> observer_engage_look{
        TEXT("/SpaceGame/Input/Observer/IA_ObserverEngageLook")};
    static ConstructorHelpers::FObjectFinder<UInputAction> observer_adjust_speed{
        TEXT("/SpaceGame/Input/Observer/IA_ObserverAdjustSpeed")};
    static ConstructorHelpers::FObjectFinder<UInputAction> observer_boost{
        TEXT("/SpaceGame/Input/Observer/IA_ObserverBoost")};
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> benchmark_mapping{
        TEXT("/SpaceGame/Input/Benchmark/IMC_Benchmark")};
    static ConstructorHelpers::FObjectFinder<UInputAction> benchmark_exit{
        TEXT("/SpaceGame/Input/Benchmark/IA_ExitBenchmark")};

    observer_input.mapping_context = observer_mapping.Object;
    observer_input.move = observer_move.Object;
    observer_input.vertical_move = observer_vertical_move.Object;
    observer_input.look = observer_look.Object;
    observer_input.engage_look = observer_engage_look.Object;
    observer_input.adjust_speed = observer_adjust_speed.Object;
    observer_input.boost = observer_boost.Object;
    benchmark_input.mapping_context = benchmark_mapping.Object;
    benchmark_input.exit = benchmark_exit.Object;
}

bool ASpaceGamePlayerController::InputKey(FInputKeyEventArgs const& event_args) {
    if (spacegame::input_trace::enabled.GetValueOnGameThread() != 0 &&
        (event_args.Key == EKeys::Gamepad_FaceButton_Bottom ||
         event_args.Key == EKeys::Gamepad_LeftY || event_args.Key == EKeys::Gamepad_RightY ||
         event_args.Key == EKeys::Gamepad_RightTriggerAxis ||
         event_args.Key == EKeys::LeftMouseButton || event_args.Key == EKeys::MouseY)) {
        auto contexts{FString{}};
        auto actions{FString{}};
        bool common_ui_click{};
        if (auto* const local_player{GetLocalPlayer()}) {
            if (auto* const common_input{local_player->GetSubsystem<UCommonInputSubsystem>()}) {
                common_ui_click = common_input->GetIsGamepadSimulatedClick();
            }
            if (auto* const subsystem{
                    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)}) {
                UInputMappingContext const* const canonical_contexts[]{global_input.mapping_context,
                                                                       input.starfox,
                                                                       input.fighter,
                                                                       input.skater,
                                                                       input.gunship};
                for (auto const* const context : canonical_contexts) {
                    if (IsValid(context) && subsystem->HasMappingContext(context)) {
                        if (!contexts.IsEmpty()) {
                            contexts += TEXT(", ");
                        }
                        contexts += context->GetName();
                    }
                }
                for (auto const& mapping : subsystem->GetAllPlayerMappableActionKeyMappings()) {
                    if (mapping.Key != event_args.Key) {
                        continue;
                    }
                    if (!actions.IsEmpty()) {
                        actions += TEXT(", ");
                    }
                    actions += GetNameSafe(mapping.Action.Get());
                }
            }
        }
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("[InputTrace] viewport key=%s event=%d value=%.3f gamepad=%d simulated=%d "
                    "common_ui_click=%d "
                    "device=%d contexts=[%s] actions=[%s]"),
               *event_args.Key.GetFName().ToString(),
               static_cast<int32>(event_args.Event),
               event_args.AmountDepressed,
               event_args.IsGamepad(),
               event_args.IsSimulatedInput(),
               common_ui_click,
               event_args.InputDevice.GetId(),
               *contexts,
               *actions);
    }
    return Super::InputKey(event_args);
}

/* **************************************** */
// Lifecycle and possession
/* **************************************** */
void ASpaceGamePlayerController::BeginPlay() {
    Super::BeginPlay();

    initialise_input_user_settings();
    if (auto* const game_instance{GetGameInstance()}; IsValid(game_instance)) {
        if (auto* const settings{game_instance->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()};
            IsValid(settings)) {
            settings->flight_model_config_changed.AddUObject(
                this, &ThisClass::apply_player_ship_flight_model_config);
        }
    }
    begin_play_finished_ = true;
    if (!is_gameplay_mode()) {
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
    if (auto* const ship{Cast<Pawn>(GetPawn())}; IsValid(ship)) {
        attach_ship(*ship);
        activate_ship_control();
    } else {
        UE_LOG(LogSandbox, Display, TEXT("ASpaceGamePlayerController::BeginPlay: No player pawn."));
    }
    initial_pause_timer_ =
        GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::show_initial_pause_menu);
}
void ASpaceGamePlayerController::show_main_menu() {
    if (ending_play_) {
        return;
    }
    mode_ = EPlayerControllerMode::MainMenu;
    if (begin_play_finished_) {
        initialise_main_menu();
    }
}
void ASpaceGamePlayerController::initialise_main_menu() {
    GetWorldTimerManager().ClearTimer(initial_pause_timer_);
    modal_ui_.clear_menus(*this);
    unbind_orchestrator_events();
    detach_ship();
    control_contexts_.shutdown();
    hud_.shutdown();
    hud_.shutdown_benchmark(*this);
    if (!modal_ui_.initialise_root(*this, ui_data) || !modal_ui_.show_main_menu(*this)) {
        return;
    }
    apply_main_menu_input_mode();
    main_menu_input_timer_ =
        GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::apply_main_menu_input_mode);
    SetActorTickEnabled(false);
}
void ASpaceGamePlayerController::apply_main_menu_input_mode() {
    if (mode_ == EPlayerControllerMode::MainMenu && !ending_play_) {
        modal_ui_.apply_main_menu_input_mode(*this);
    }
}
void ASpaceGamePlayerController::EndPlay(EEndPlayReason::Type const reason) {
    ending_play_ = true;
    if (auto* const local_player{GetLocalPlayer()}; IsValid(local_player)) {
        if (auto* const subsystem{
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)};
            IsValid(subsystem)) {
            subsystem->OnPostUserSettingsInitialized.RemoveDynamic(
                this, &ThisClass::on_input_user_settings_initialized);
        }
    }
    registered_input_settings_.Reset();
    GetWorldTimerManager().ClearTimer(initial_pause_timer_);
    GetWorldTimerManager().ClearTimer(main_menu_input_timer_);
    modal_ui_.detach_callbacks(*this);
    unbind_orchestrator_events();
    detach_ship();
    control_contexts_.shutdown();
    hud_.shutdown();
    hud_.shutdown_benchmark(*this);
    modal_ui_.shutdown(*this);
    if (auto* const game_instance{GetGameInstance()}; IsValid(game_instance)) {
        if (auto* const settings{game_instance->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()};
            IsValid(settings)) {
            settings->flight_model_config_changed.RemoveAll(this);
        }
    }
    Super::EndPlay(reason);
}
void ASpaceGamePlayerController::OnPossess(APawn* const in_pawn) {
    Super::OnPossess(in_pawn);
    auto* const ship{Cast<Pawn>(in_pawn)};
    if (!IsValid(ship)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ASpaceGamePlayerController::OnPossess: Player ship is invalid."));
        SetActorTickEnabled(false);
        return;
    }
    if (!is_gameplay_mode()) {
        return;
    }
    attach_ship(*ship);
    activate_ship_control();
    SetActorTickEnabled(true);
    UE_LOG(LogSandbox, Display, TEXT("Possessed player ship"));
}
void ASpaceGamePlayerController::attach_ship(Pawn& ship) {
    ship.on_player_ship_died.BindUObject(this, &ThisClass::on_player_ship_died);
    apply_player_ship_flight_model_config(ship);
    control_contexts_.set_ship(&ship);
    modal_ui_.on_ship_changed(true);

    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>()
                                            : nullptr};
    if (IsValid(game)) {
        game->start_player_ship_ambience();
    } else {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::attach_ship: Game subsystem is unavailable; "
                    "player ship ambience will not play."));
    }
}
void ASpaceGamePlayerController::apply_player_ship_flight_model_config() {
    if (auto* const ship{Cast<Pawn>(GetPawn())}; IsValid(ship)) {
        apply_player_ship_flight_model_config(*ship);
    }
}

void ASpaceGamePlayerController::apply_player_ship_flight_model_config(Pawn& ship) const {
    auto* const game_instance{GetGameInstance()};
    auto* const settings{IsValid(game_instance)
                             ? game_instance->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()
                             : nullptr};
    if (!IsValid(settings)) {
        return;
    }

    auto const& loadout{settings->flight_model_loadout()};
    using ::ioj::sim::player::FlightModelSlot;
    for (auto const slot : {FlightModelSlot::Up,
                            FlightModelSlot::Right,
                            FlightModelSlot::Down,
                            FlightModelSlot::Left}) {
        if (!ship.set_flight_model_slot_profile(
                slot, ::ioj::sim::player::flight_model_profile(loadout, slot))) {
            UE_LOG(LogSandboxController, Error, TEXT("Could not apply saved flight model slot"));
        }
    }
}
void ASpaceGamePlayerController::on_player_ship_flight_model_selected() {
    auto* const ship{Cast<Pawn>(GetPawn())};
    if (!IsValid(ship)) {
        return;
    }
    if (auto* const orchestrator{orchestrator_.Get()}; IsValid(orchestrator)) {
        auto const scope{ml::ioj::flight_control_scope(ship->get_active_flight_model_slot())};
        orchestrator->get_hud_manager().set_selected_mapping_context(
            FString{ml::ioj::canonical_ship_control_context(scope).asset_name});
    }
}
void ASpaceGamePlayerController::activate_ship_control() {
    if (!begin_play_finished_ || !can_activate_gameplay_control() ||
        !control_contexts_.can_bind_context(EPlayerControlContext::Player)) {
        return;
    }
    bind_orchestrator_events();
    modal_ui_.initialise_root(*this, ui_data);
    control_contexts_.set_control_context(EPlayerControlContext::Player);
    hud_.initialise(*this,
                    orchestrator_.Get(),
                    ui_data,
                    EPlayerControlContext::Player,
                    control_contexts_.get_observer_speed());
}
void ASpaceGamePlayerController::OnUnPossess() {
    detach_ship();
    SetActorTickEnabled(false);
    UE_LOG(LogSandbox, Display, TEXT("Unpossessed player ship"));
    Super::OnUnPossess();
}
void ASpaceGamePlayerController::detach_ship() {
    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>()
                                            : nullptr};
    if (IsValid(game)) {
        game->stop_player_ship_ambience();
    }

    if (auto* const ship{Cast<Pawn>(GetPawn())}; IsValid(ship)) {
        ship->on_player_ship_died.Unbind();
    }
    control_contexts_.set_ship(nullptr);
    modal_ui_.on_ship_changed(false);
}
void ASpaceGamePlayerController::on_player_ship_died() {
    UnPossess();
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
            Warning,
            TEXT("ASpaceGamePlayerController::bind_orchestrator_events: Orchestrator is invalid."));
        return;
    }

    if (orchestrator_.Get() == orchestrator) {
        return;
    }
    unbind_orchestrator_events();
    orchestrator_ = orchestrator;
    orchestrator->on_reset.RemoveAll(this);
    orchestrator->on_reset.AddUObject(this, &ThisClass::on_orchestrator_reset);
    orchestrator->on_mission_completed.RemoveAll(this);
    orchestrator->on_mission_completed.AddUObject(this, &ThisClass::on_mission_completed);
}
void ASpaceGamePlayerController::unbind_orchestrator_events() {
    if (auto* const orchestrator{orchestrator_.Get()}; IsValid(orchestrator)) {
        orchestrator->on_reset.RemoveAll(this);
        orchestrator->on_mission_completed.RemoveAll(this);
    }
    orchestrator_.Reset();
}
void ASpaceGamePlayerController::on_orchestrator_reset(ATestBatchOrchestrator& orchestrator) {
    if (!is_gameplay_mode()) {
        return;
    }
    modal_ui_.clear_modals(*this);
    hud_.restore_after_modal();
    return_to_level_select_pending_ = false;
    control_contexts_.set_control_context(EPlayerControlContext::None);
    hud_.shutdown();
    hud_.shutdown_benchmark(*this);

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
        attach_ship(*player_ship);
        activate_ship_control();
    }
}

/* **************************************** */
// Input and presentation callbacks
/* **************************************** */
void ASpaceGamePlayerController::SetupInputComponent() {
    Super::SetupInputComponent();

    if (!is_gameplay_mode()) {
        return;
    }

    TRY_INIT_PTR(input_component, Cast<UEnhancedInputComponent>(InputComponent));
    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(input_subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));

    initialise_input_user_settings();

    if (control_contexts_.initialise(*this,
                                     *input_component,
                                     *input_subsystem,
                                     input,
                                     observer_input,
                                     benchmark_input,
                                     global_input)) {
        activate_ship_control();
    }
}
void ASpaceGamePlayerController::initialise_input_user_settings() {
    auto* const local_player{GetLocalPlayer()};
    if (!IsValid(local_player)) {
        return;
    }
    auto* const subsystem{
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_input_user_settings: Enhanced Input "
                    "subsystem is invalid."));
        return;
    }
    subsystem->OnPostUserSettingsInitialized.AddUniqueDynamic(
        this, &ThisClass::on_input_user_settings_initialized);
    auto* const settings{subsystem->GetUserSettings()};
    if (!IsValid(settings)) {
        return;
    }
    if (registered_input_settings_.Get() == settings) {
        return;
    }
    for (auto const& definition : ml::ioj::canonical_ship_control_contexts()) {
        auto* const mapping{ml::ioj::load_ship_control_context(definition.scope)};
        if (!IsValid(mapping)) {
            UE_LOG(LogSandboxController,
                   Error,
                   TEXT("Could not register canonical input context %s"),
                   definition.asset_name);
            return;
        }
        if (!settings->IsMappingContextRegistered(mapping)) {
            settings->RegisterInputMappingContext(mapping);
        }
    }
    if (auto* const ship_settings{Cast<ml::ioj::USpaceGameInputUserSettings>(settings)}) {
        ship_settings->finalize_canonical_registration();
    } else {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("Canonical input settings class is not installed for the local player"));
        return;
    }

    registered_input_settings_ = settings;
    FModifyContextOptions rebuild_options;
    rebuild_options.bForceImmediately = true;
    subsystem->RequestRebuildControlMappings(rebuild_options,
                                             EInputMappingRebuildType::RebuildWithFlush);
}
void ASpaceGamePlayerController::on_input_user_settings_initialized(
    UEnhancedInputUserSettings const* const settings) {
    static_cast<void>(settings);
    initialise_input_user_settings();
}
auto ASpaceGamePlayerController::activate_playerless_camera(ACameraActor& camera,
                                                            EPlayerControlContext const context)
    -> bool {
    if (!can_activate_gameplay_control()) {
        return false;
    }
    if (context != EPlayerControlContext::Observer && context != EPlayerControlContext::Benchmark) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::activate_playerless_camera: Context must be "
                    "Observer or Benchmark."));
        return false;
    }
    if (IsValid(GetPawn())) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::activate_playerless_camera: Controller still has "
                    "a pawn."));
        return false;
    }

    bind_orchestrator_events();
    control_contexts_.set_camera(&camera);
    SetViewTarget(&camera);
    if (!control_contexts_.set_control_context(context)) {
        return false;
    }

    if (context == EPlayerControlContext::Observer) {
        hud_.shutdown_benchmark(*this);
        if (!modal_ui_.initialise_root(*this, ui_data) ||
            !hud_.initialise(*this,
                             orchestrator_.Get(),
                             ui_data,
                             EPlayerControlContext::Observer,
                             control_contexts_.get_observer_speed())) {
            control_contexts_.set_control_context(EPlayerControlContext::None);
            return false;
        }
        FInputModeGameAndUI input_mode{};
        input_mode.SetHideCursorDuringCapture(false);
        input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(input_mode);
        SetShowMouseCursor(true);
    } else {
        hud_.shutdown();
        modal_ui_.shutdown(*this);
        if (!hud_.initialise_benchmark(*this, orchestrator_.Get(), ui_data)) {
            control_contexts_.set_control_context(EPlayerControlContext::None);
            return false;
        }
    }
    SetActorTickEnabled(true);
    return true;
}
void ASpaceGamePlayerController::set_observer_look_active(bool const active) {
    if (active) {
        FInputModeGameOnly input_mode{};
        SetInputMode(input_mode);
        SetShowMouseCursor(false);
        return;
    }

    if (get_active_control_context() == EPlayerControlContext::Observer) {
        FInputModeGameAndUI input_mode{};
        input_mode.SetHideCursorDuringCapture(false);
        input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(input_mode);
        SetShowMouseCursor(true);
    }
}
void ASpaceGamePlayerController::set_observer_movement_speed(float const speed) {
    hud_.set_observer_speed(speed);
}

/* **************************************** */
// Modal and simulation transitions
/* **************************************** */
void ASpaceGamePlayerController::toggle_pause_game() {
    if (!is_gameplay_mode()) {
        return;
    }
    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::toggle_pause_game: Orchestrator is invalid."));
        return;
    }

    switch (orchestrator->get_state()) {
        case ::ioj::sim::OrchestratorState::Running: {
            if (return_to_level_select_pending_ || (modal_ui_.is_completion_active())) {
                return;
            }
            if (!suspend_gameplay_for_modal()) {
                return;
            }

            if (!open_pause_menu()) {
                resume_game();
            }
            break;
        }
        case ::ioj::sim::OrchestratorState::Uninitialised:
        case ::ioj::sim::OrchestratorState::Paused: {
            if (modal_ui_.is_pause_active()) {
                modal_ui_.deactivate_pause();
            } else if (!modal_ui_.has_completion()) {
                resume_game();
            }
            break;
        }
        case ::ioj::sim::OrchestratorState::Stopped: {
            UE_LOG(LogSandboxController,
                   Error,
                   TEXT("ASpaceGamePlayerController::toggle_pause_game: Orchestrator is stopped."));
            break;
        }
    }
}
void ASpaceGamePlayerController::show_initial_pause_menu() {
    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator) || !orchestrator->was_launched_paused() ||
        orchestrator->get_state() != ::ioj::sim::OrchestratorState::Paused || !is_gameplay_mode() ||
        get_active_control_context() == EPlayerControlContext::Benchmark ||
        return_to_level_select_pending_ || modal_ui_.has_modal()) {
        return;
    }

    auto const restore_context{get_active_control_context() == EPlayerControlContext::None &&
                                       IsValid(Cast<Pawn>(GetPawn()))
                                   ? EPlayerControlContext::Player
                                   : get_active_control_context()};
    modal_ui_.begin_suspend(restore_context);
    if (!control_contexts_.set_control_context(EPlayerControlContext::None)) {
        modal_ui_.clear_resume();
        return;
    }

    hud_.hide_for_modal();
    if (!open_pause_menu()) {
        resume_game();
    }
}
auto ASpaceGamePlayerController::open_pause_menu() -> bool {
    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator) ||
        orchestrator->get_state() != ::ioj::sim::OrchestratorState::Paused ||
        !modal_ui_.has_root() || !IsValid(global_input.toggle_menu) || !IsValid(ui_data) ||
        !IsValid(ui_data->team_visual_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::open_pause_menu: Paused gameplay UI is not "
                    "available."));
        return false;
    }

    auto& hud_manager{orchestrator->get_hud_manager()};
    hud_manager.force_sample();
    auto const& entity_counts{hud_manager.get_entity_count_data()};
    auto const& kill_data{hud_manager.get_kill_data()};

    ml::ioj::FPauseMenuData pause_data;
    pause_data.telemetry = orchestrator->get_level_telemetry_manager().make_snapshot();
    pause_data.alive_per_team_and_type = entity_counts.alive_per_team_and_type;
    pause_data.top_killers = kill_data.top_killers;
    pause_data.team_kill_matrix = kill_data.team_kill_matrix;
    pause_data.team_colours = ui_data->team_visual_data->build_team_colour_cache();
    return modal_ui_.show_pause(*this, *global_input.toggle_menu, MoveTemp(pause_data));
}
auto ASpaceGamePlayerController::suspend_gameplay_for_modal() -> bool {
    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator) ||
        orchestrator->get_state() != ::ioj::sim::OrchestratorState::Running) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::suspend_gameplay_for_modal: Gameplay is not "
                    "running."));
        return false;
    }

    modal_ui_.begin_suspend(get_active_control_context());
    orchestrator->pause_simulation();
    if (!control_contexts_.set_control_context(EPlayerControlContext::None)) {
        modal_ui_.clear_resume();
        orchestrator->start_simulation();
        return false;
    }
    hud_.hide_for_modal();
    return true;
}
void ASpaceGamePlayerController::resume_game() {
    if (!is_gameplay_mode() || return_to_level_select_pending_) {
        return;
    }
    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::resume_game: Orchestrator is invalid."));
        return;
    }
    if (orchestrator->get_state() == ::ioj::sim::OrchestratorState::Stopped) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::resume_game: Orchestrator is stopped."));
        return;
    }

    auto const target_context{modal_ui_.resume_context(IsValid(Cast<Pawn>(GetPawn()))
                                                           ? EPlayerControlContext::Player
                                                           : EPlayerControlContext::None)};
    if (!control_contexts_.set_control_context(target_context)) {
        return;
    }
    hud_.restore_after_modal();
    modal_ui_.clear_resume();
    if (target_context == EPlayerControlContext::Player) {
        hud_.initialise(
            *this, orchestrator, ui_data, target_context, control_contexts_.get_observer_speed());
    }

    switch (orchestrator->get_state()) {
        case ::ioj::sim::OrchestratorState::Uninitialised:
        case ::ioj::sim::OrchestratorState::Paused: {
            orchestrator->start_simulation();
            break;
        }
        case ::ioj::sim::OrchestratorState::Running:
        case ::ioj::sim::OrchestratorState::Stopped: {
            break;
        }
    }
}
void ASpaceGamePlayerController::on_pause_menu_deactivated() {
    modal_ui_.close_pause(*this);
    if (is_gameplay_mode() && !return_to_level_select_pending_) {
        resume_game();
    }
}
void ASpaceGamePlayerController::on_completion_menu_deactivated() {
    modal_ui_.close_completion(*this);
    if (is_gameplay_mode() && !return_to_level_select_pending_) {
        resume_game();
    }
}
void ASpaceGamePlayerController::on_mission_completed(FTestMissionCompletion const& completion) {
    if (!is_gameplay_mode()) {
        return;
    }
    if (!completion.persisted || return_to_level_select_pending_ || modal_ui_.has_modal()) {
        return;
    }
    if (!modal_ui_.has_root() || !suspend_gameplay_for_modal()) {
        return;
    }

    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::on_mission_completed: Orchestrator is invalid."));
        resume_game();
        return;
    }

    if (!modal_ui_.show_completion(
            *this, completion, orchestrator->get_level_telemetry_manager().make_snapshot())) {
        resume_game();
    }
}
void ASpaceGamePlayerController::return_to_level_select() {
    if (!is_gameplay_mode()) {
        return;
    }
    if (return_to_level_select_pending_) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{ml::ioj::UGameSubsystem::get(game_instance)};
    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(subsystem) || !IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::return_to_level_select: Required gameplay "
                    "objects are invalid."));
        return;
    }

    return_to_level_select_pending_ = true;
    auto const preferred_level_id{orchestrator->get_mission_manager().get_level_id()};
    if (!subsystem->return_to_level_select(FName{UTF8_TO_TCHAR(preferred_level_id.c_str())})) {
        return_to_level_select_pending_ = false;
        return;
    }

    modal_ui_.clear_menus(*this);
    hud_.cancel_restore();
    hud_.shutdown_benchmark(*this);
    control_contexts_.set_control_context(EPlayerControlContext::None);
}
void ASpaceGamePlayerController::quit_game() {
    if (!is_gameplay_mode()) {
        return;
    }
    if (return_to_level_select_pending_) {
        return;
    }
    return_to_level_select_pending_ = true;
    modal_ui_.detach_callbacks(*this);
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

/* **************************************** */
// Diagnostics
/* **************************************** */
void ASpaceGamePlayerController::Tick(float const dt) {
    Super::Tick(dt);

    log_config.tick(dt);
    screenshot_tick(dt);
    log_config.on_tick_end();
}
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
