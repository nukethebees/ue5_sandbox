#include <SpaceGame/ships/player/SpaceGamePlayerController.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SpaceGame/input/ControlProfiles.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/presentation/widgets/BattleViewerHudWidget.h>
#include <SpaceGame/presentation/widgets/BenchmarkHudWidget.h>
#include <SpaceGame/presentation/widgets/ShipHudWidget.h>
#include <SpaceGame/presentation/widgets/SimulationHudWidget.h>
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
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <Kismet/KismetSystemLibrary.h>
#include <TimerManager.h>
#include <UnrealClient.h>
#include <UObject/ConstructorHelpers.h>
#include <UserSettings/EnhancedInputUserSettings.h>

#include <SandboxGameShared/utilities/macros/null_checks.hpp>

namespace {
constexpr int32 global_mapping_priority{100};
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
    if (!observer_control_context_.initialise(
            *this, *input_component, *input_subsystem, observer_input)) {
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
    global_mapping_enabled_ = true;
    return true;
}

void ASpaceGamePlayerController::shutdown_global_input() {
    if (!global_input_bound_) {
        return;
    }

    set_global_mapping_enabled(false);
    if (auto* const input_component{global_input_component_.Get()}; IsValid(input_component)) {
        input_component->RemoveBindingByHandle(global_input_binding_handle_);
    }

    global_input_binding_handle_ = 0;
    global_input_subsystem_.Reset();
    global_input_component_.Reset();
    global_input_bound_ = false;
}

void ASpaceGamePlayerController::set_global_mapping_enabled(bool const enabled) {
    if (!global_input_bound_ || global_mapping_enabled_ == enabled) {
        return;
    }
    auto* const input_subsystem{global_input_subsystem_.Get()};
    if (!IsValid(input_subsystem) || !IsValid(global_input.mapping_context)) {
        return;
    }

    if (enabled) {
        input_subsystem->AddMappingContext(global_input.mapping_context, global_mapping_priority);
    } else {
        input_subsystem->RemoveMappingContext(global_input.mapping_context);
    }
    global_mapping_enabled_ = enabled;
}

auto ASpaceGamePlayerController::can_bind_context(EPlayerControlContext const context) const
    -> bool {
    switch (context) {
        case EPlayerControlContext::None: {
            return true;
        }
        case EPlayerControlContext::Player: {
            return ship_control_context_.can_bind();
        }
        case EPlayerControlContext::Observer: {
            return observer_control_context_.can_bind();
        }
        case EPlayerControlContext::Benchmark: {
            return IsValid(benchmark_input.mapping_context) && IsValid(benchmark_input.exit) &&
                   global_input_component_.IsValid() && global_input_subsystem_.IsValid();
        }
    }
    return false;
}

auto ASpaceGamePlayerController::bind_context(EPlayerControlContext const context) -> bool {
    switch (context) {
        case EPlayerControlContext::None: {
            return true;
        }
        case EPlayerControlContext::Player: {
            set_global_mapping_enabled(true);
            return ship_control_context_.bind();
        }
        case EPlayerControlContext::Observer: {
            set_global_mapping_enabled(true);
            return observer_control_context_.bind();
        }
        case EPlayerControlContext::Benchmark: {
            return bind_benchmark_context();
        }
    }
    return false;
}

void ASpaceGamePlayerController::unbind_context(EPlayerControlContext const context) {
    switch (context) {
        case EPlayerControlContext::None: {
            break;
        }
        case EPlayerControlContext::Player: {
            ship_control_context_.unbind();
            break;
        }
        case EPlayerControlContext::Observer: {
            observer_control_context_.unbind();
            break;
        }
        case EPlayerControlContext::Benchmark: {
            unbind_benchmark_context();
            break;
        }
    }
}

auto ASpaceGamePlayerController::bind_benchmark_context() -> bool {
    auto* const input_component{global_input_component_.Get()};
    auto* const input_subsystem{global_input_subsystem_.Get()};
    if (!IsValid(input_component) || !IsValid(input_subsystem) ||
        !IsValid(benchmark_input.mapping_context) || !IsValid(benchmark_input.exit)) {
        return false;
    }

    set_global_mapping_enabled(false);
    auto& binding{input_component->BindAction(
        benchmark_input.exit, ETriggerEvent::Started, this, &ThisClass::exit_benchmark)};
    benchmark_exit_binding_handle_ = binding.GetHandle();
    input_subsystem->AddMappingContext(benchmark_input.mapping_context, global_mapping_priority);
    return true;
}

void ASpaceGamePlayerController::unbind_benchmark_context() {
    if (auto* const input_subsystem{global_input_subsystem_.Get()};
        IsValid(input_subsystem) && IsValid(benchmark_input.mapping_context)) {
        input_subsystem->RemoveMappingContext(benchmark_input.mapping_context);
    }
    if (auto* const input_component{global_input_component_.Get()}; IsValid(input_component)) {
        input_component->RemoveBindingByHandle(benchmark_exit_binding_handle_);
    }
    benchmark_exit_binding_handle_ = 0;
}

void ASpaceGamePlayerController::exit_benchmark() {
    return_to_level_select();
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

void ASpaceGamePlayerController::on_ship_control_profile_changed(FString const& profile_name) {
    UE_LOG(LogSandbox, Display, TEXT("Setting control profile to: %s"), *profile_name);

    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::on_ship_control_profile_changed: HUD "
                    "orchestrator is invalid."));
        return;
    }
    orchestrator->get_hud_manager().set_selected_mapping_context(profile_name);
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
            if (!suspend_gameplay_for_modal()) {
                return;
            }

            if (!open_pause_menu()) {
                resume_game();
            }
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

    initialise_input_user_settings();
    begin_play_finished_ = true;
    if (main_menu_requested_) {
        initialise_main_menu();
        return;
    }

    initialise_gameplay();
}

void ASpaceGamePlayerController::initialise_input_user_settings() {
    auto* const local_player{GetLocalPlayer()};
    auto* const subsystem{
        IsValid(local_player)
            ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)
            : nullptr};
    auto* const settings{IsValid(subsystem) ? subsystem->GetUserSettings() : nullptr};
    auto* const mapping_context{input.get_mapping_context()};
    if (!IsValid(settings) || !IsValid(mapping_context)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_input_user_settings: Input settings "
                    "or mapping context are invalid."));
        return;
    }
    ml::ioj::register_control_profiles(*settings, *mapping_context);
}

void ASpaceGamePlayerController::initialise_gameplay() {
    FInputModeGameOnly input_mode{};
    SetInputMode(input_mode);
    SetShowMouseCursor(false);

    bind_orchestrator_events();

    auto* const ship{Cast<Pawn>(GetPawn())};
    if (IsValid(ship)) {
        initialise_ui_root();
        ship_control_context_.set_ship(ship);
        set_control_context(EPlayerControlContext::Player);
        initialise_hud(EPlayerControlContext::Player);
    } else {
        UE_LOG(LogSandbox, Display, TEXT("ASpaceGamePlayerController::BeginPlay: No player pawn."));
    }

    GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::show_initial_pause_menu);
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
    shutdown_benchmark_hud();
    set_control_context(EPlayerControlContext::None);
    observer_control_context_.shutdown();
    ship_control_context_.shutdown();
    shutdown_global_input();

    if (auto* const orchestrator{hud_orchestrator.Get()}; IsValid(orchestrator)) {
        orchestrator->on_reset.RemoveAll(this);
        orchestrator->on_mission_completed.RemoveAll(this);
    }

    shutdown_hud();
    hud_orchestrator.Reset();

    Super::EndPlay(reason);
}

/* ---------------------------------------------------------------------------------------------- */
// Pawn possession
/* ---------------------------------------------------------------------------------------------- */
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

    ship->on_player_ship_died.BindUObject(this, &ThisClass::on_player_ship_died);
    ship_control_context_.set_ship(ship);
    if (begin_play_finished_ && !IsValid(pause_menu) && !main_menu_requested_) {
        initialise_ui_root();
        set_control_context(EPlayerControlContext::Player);
        initialise_hud(EPlayerControlContext::Player);
    }

    SetActorTickEnabled(true);
    UE_LOG(LogSandbox, Display, TEXT("Possessed player ship"));
}

void ASpaceGamePlayerController::OnUnPossess() {
    if (auto* const ship{Cast<Pawn>(GetPawn())}) {
        ship->on_player_ship_died.Unbind();
    }

    if (active_control_context_ == EPlayerControlContext::Player) {
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
    shutdown_hud();
    shutdown_benchmark_hud();
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
    auto const show_telemetry{level_select_request.IsSet() &&
                              level_select_request->destination ==
                                  ml::ioj::EMainMenuDestination::Telemetry};
    auto const telemetry_run_id{
        level_select_request.IsSet() ? level_select_request->selected_telemetry_run_id : FString{}};
    auto const telemetry_error{level_select_request.IsSet() ? level_select_request->telemetry_error
                                                            : FString{}};
    if (!ui_root->show_main_menu(show_level_select,
                                 preferred_level_id,
                                 show_telemetry,
                                 telemetry_run_id,
                                 telemetry_error)) {
        return;
    }

    apply_main_menu_input_mode();
    GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::apply_main_menu_input_mode);
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
    modal_restore_context_ = EPlayerControlContext::None;
    modal_resume_pending_ = false;
    return_to_level_select_pending_ = false;
    set_control_context(EPlayerControlContext::None);
    shutdown_hud();
    shutdown_benchmark_hud();

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
        set_control_context(EPlayerControlContext::Player);
        initialise_hud(EPlayerControlContext::Player);
    }
}

auto ASpaceGamePlayerController::activate_playerless_camera(ACameraActor& camera,
                                                            EPlayerControlContext const context)
    -> bool {
    if (main_menu_requested_) {
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

    observer_control_context_.set_camera(&camera);
    SetViewTarget(&camera);
    if (!set_control_context(context)) {
        return false;
    }

    if (context == EPlayerControlContext::Observer) {
        shutdown_benchmark_hud();
        if (!initialise_ui_root() || !initialise_hud(EPlayerControlContext::Observer)) {
            set_control_context(EPlayerControlContext::None);
            return false;
        }
        FInputModeGameAndUI input_mode{};
        input_mode.SetHideCursorDuringCapture(false);
        input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(input_mode);
        SetShowMouseCursor(true);
    } else {
        shutdown_hud();
        shutdown_ui_root();
        if (!initialise_benchmark_hud()) {
            set_control_context(EPlayerControlContext::None);
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

    if (active_control_context_ == EPlayerControlContext::Observer) {
        FInputModeGameAndUI input_mode{};
        input_mode.SetHideCursorDuringCapture(false);
        input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(input_mode);
        SetShowMouseCursor(true);
    }
}

void ASpaceGamePlayerController::set_observer_movement_speed(float const speed) {
    if (auto* const viewer_hud{Cast<UBattleViewerHudWidget>(hud_widget)}; IsValid(viewer_hud)) {
        viewer_hud->set_movement_speed(speed);
    }
}

auto ASpaceGamePlayerController::initialise_hud(EPlayerControlContext const context) -> bool {
    if (IsValid(hud_widget)) {
        auto const correct_type{context == EPlayerControlContext::Player
                                    ? hud_widget->IsA<UShipHudWidget>()
                                    : context == EPlayerControlContext::Observer &&
                                          hud_widget->IsA<UBattleViewerHudWidget>()};
        if (correct_type) {
            return true;
        }
        shutdown_hud();
    }

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_hud: World is not available yet."));
        return false;
    }
    if (!IsValid(GetLocalPlayer())) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_hud: Local player is not available "
                    "yet."));
        return false;
    }

    auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_hud: Orchestrator is not available "
                    "yet."));
        return false;
    }
    if (!IsValid(ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_hud: UI data is invalid."));
        return false;
    }
    hud_orchestrator = orchestrator;

    auto* const team_visual_data{ui_data->team_visual_data.Get()};
    if (!IsValid(team_visual_data)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_hud: Team visual data is invalid."));
        return false;
    }

    TSubclassOf<USimulationHudWidget> hud_widget_class;
    FName widget_name;
    if (context == EPlayerControlContext::Player) {
        hud_widget_class = ui_data->get_widget_class<UShipHudWidget>();
        widget_name = TEXT("ship_hud");
    } else if (context == EPlayerControlContext::Observer) {
        hud_widget_class = ui_data->get_widget_class<UBattleViewerHudWidget>();
        widget_name = TEXT("battle_viewer_hud");
    } else {
        return false;
    }
    if (!hud_widget_class) {
        return false;
    }

    auto* const created_widget{
        CreateWidget<USimulationHudWidget>(this, hud_widget_class, widget_name)};
    if (!IsValid(created_widget)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_hud: Failed to create HUD widget."));
        return false;
    }

    hud_widget = created_widget;
    auto* const game_subsystem{GetGameInstance()->GetSubsystem<ml::ioj::UGameSubsystem>()};
    if (IsValid(game_subsystem)) {
        created_widget->apply_ui_style(game_subsystem->get_ui_style());
    } else {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_hud: Game subsystem is invalid."));
    }
    created_widget->AddToViewport();
    created_widget->set_entity_colours(team_visual_data->build_team_colour_cache());
    if (auto* const ship_hud{Cast<UShipHudWidget>(created_widget)}; IsValid(ship_hud)) {
        ship_hud->set_crosshair_distances(ui_data->crosshair_distances);
    }
    if (auto* const viewer_hud{Cast<UBattleViewerHudWidget>(created_widget)}; IsValid(viewer_hud)) {
        viewer_hud->set_movement_speed(observer_control_context_.get_movement_speed());
    }
    orchestrator->get_hud_manager().register_hud(*created_widget);
    return true;
}

void ASpaceGamePlayerController::shutdown_hud() {
    if (!IsValid(hud_widget)) {
        return;
    }
    if (auto* const orchestrator{hud_orchestrator.Get()};
        IsValid(orchestrator) && orchestrator->get_hud_manager().get_registered_hud_count() > 0) {
        orchestrator->get_hud_manager().unregister_hud(*hud_widget);
    }
    hud_widget->RemoveFromParent();
    hud_widget = nullptr;
    hud_restore_pending_ = false;
}

auto ASpaceGamePlayerController::initialise_benchmark_hud() -> bool {
    if (IsValid(benchmark_hud_widget)) {
        return true;
    }
    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_benchmark_hud: World is not "
                    "available yet."));
        return false;
    }
    auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::initialise_benchmark_hud: Orchestrator is not "
                    "available yet."));
        return false;
    }
    if (!IsValid(ui_data)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_benchmark_hud: UI data is invalid."));
        return false;
    }
    hud_orchestrator = orchestrator;

    auto const widget_class{ui_data->get_widget_class<UBenchmarkHudWidget>()};
    if (!widget_class) {
        return false;
    }
    auto* const created_widget{
        CreateWidget<UBenchmarkHudWidget>(this, widget_class, TEXT("benchmark_hud"))};
    if (!IsValid(created_widget)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::initialise_benchmark_hud: Failed to create "
                    "widget."));
        return false;
    }

    benchmark_hud_widget = created_widget;
    if (auto* const game_subsystem{GetGameInstance()->GetSubsystem<ml::ioj::UGameSubsystem>()};
        IsValid(game_subsystem)) {
        created_widget->apply_ui_style(game_subsystem->get_ui_style());
    }
    created_widget->set_orchestrator(*orchestrator);
    created_widget->end_requested.AddUObject(this, &ThisClass::return_to_level_select);
    created_widget->AddToPlayerScreen(100);
    created_widget->ActivateWidget();
    return true;
}

void ASpaceGamePlayerController::shutdown_benchmark_hud() {
    if (!IsValid(benchmark_hud_widget)) {
        return;
    }
    benchmark_hud_widget->end_requested.RemoveAll(this);
    benchmark_hud_widget->DeactivateWidget();
    benchmark_hud_widget->RemoveFromParent();
    benchmark_hud_widget = nullptr;
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

void ASpaceGamePlayerController::show_initial_pause_menu() {
    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator) || !orchestrator->was_launched_paused() ||
        orchestrator->get_state() != EOrchestratorState::Paused || main_menu_requested_ ||
        active_control_context_ == EPlayerControlContext::Benchmark ||
        return_to_level_select_pending_ || IsValid(pause_menu) || IsValid(completion_menu)) {
        return;
    }

    modal_restore_context_ = active_control_context_;
    if (modal_restore_context_ == EPlayerControlContext::None && IsValid(Cast<Pawn>(GetPawn()))) {
        modal_restore_context_ = EPlayerControlContext::Player;
    }
    modal_resume_pending_ = true;
    if (!set_control_context(EPlayerControlContext::None)) {
        modal_restore_context_ = EPlayerControlContext::None;
        modal_resume_pending_ = false;
        return;
    }

    hide_hud_for_modal();
    if (!open_pause_menu()) {
        resume_game();
    }
}

auto ASpaceGamePlayerController::open_pause_menu() -> bool {
    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator) || orchestrator->get_state() != EOrchestratorState::Paused ||
        !IsValid(ui_root) || !IsValid(global_input.toggle_menu) || !IsValid(ui_data) ||
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
    pause_menu = ui_root->show_pause_menu(*global_input.toggle_menu, MoveTemp(pause_data));
    if (!IsValid(pause_menu)) {
        return false;
    }

    pause_menu->return_to_level_select_requested.RemoveAll(this);
    pause_menu->return_to_level_select_requested.AddUObject(this,
                                                            &ThisClass::return_to_level_select);
    pause_menu->quit_requested.RemoveAll(this);
    pause_menu->quit_requested.AddUObject(this, &ThisClass::quit_game);
    pause_menu->OnDeactivated().RemoveAll(this);
    pause_menu->OnDeactivated().AddUObject(this, &ThisClass::on_pause_menu_deactivated);
    return true;
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

    modal_restore_context_ = active_control_context_;
    modal_resume_pending_ = true;
    orchestrator->pause_simulation();
    if (!set_control_context(EPlayerControlContext::None)) {
        modal_restore_context_ = EPlayerControlContext::None;
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

    auto const target_context{modal_resume_pending_            ? modal_restore_context_
                              : IsValid(Cast<Pawn>(GetPawn())) ? EPlayerControlContext::Player
                                                               : EPlayerControlContext::None};
    if (!set_control_context(target_context)) {
        return;
    }
    restore_hud_after_modal();
    modal_restore_context_ = EPlayerControlContext::None;
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

    auto* const orchestrator{hud_orchestrator.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::on_mission_completed: Orchestrator is invalid."));
        resume_game();
        return;
    }

    auto stats_snapshot{orchestrator->get_level_telemetry_manager().make_snapshot()};
    completion_menu = ui_root->show_level_completion(
        completion.level_display_name, completion.state, MoveTemp(stats_snapshot));
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
    shutdown_benchmark_hud();
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
