#include <SpaceGame/ships/player/SpaceGamePlayerController.h>

#include <ioj/host/messages.h>
#include <SandboxGameShared/utilities/macros/null_checks.hpp>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGame/UnrealEndpoint.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/strings.h>

#include <Camera/CameraActor.h>
#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <Engine/GameViewportClient.h>
#include <Kismet/KismetSystemLibrary.h>
#include <TimerManager.h>
#include <UnrealClient.h>

namespace ioj {
ASpaceGamePlayerController::ASpaceGamePlayerController() {
    SetShowMouseCursor(true);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ASpaceGamePlayerController::BeginPlay() {
    Super::BeginPlay();

    lifecycle_ = Lifecycle::Playing;
    if (!is_gameplay_mode()) {
        initialise_main_menu();
        return;
    }

    initialise_gameplay();
}
void ASpaceGamePlayerController::initialise_gameplay() {
    find_orchestrator();
    if (auto* const ship{Cast<Pawn>(GetPawn())}; IsValid(ship)) {
        attach_ship(*ship);
        activate_ship_control();
    } else {
        UE_LOG(LogSandbox, Display, TEXT("ASpaceGamePlayerController::BeginPlay: No player pawn."));
    }
}
void ASpaceGamePlayerController::show_main_menu() {
    if (lifecycle_ == Lifecycle::Ending) {
        return;
    }

    mode_ = EPlayerControllerMode::MainMenu;
    if (lifecycle_ == Lifecycle::Playing) {
        initialise_main_menu();
    }
}
void ASpaceGamePlayerController::initialise_main_menu() {
    modal_ui_.clear_menus(*this);
    orchestrator_.Reset();
    detach_ship();
    set_control_context(levels::ControlContext::None);
    hud_.shutdown();
    hud_.shutdown_benchmark();
    if (!modal_ui_.initialise_root(*this, ui_data) || !modal_ui_.show_main_menu()) {
        return;
    }

    SetActorTickEnabled(false);
}
void ASpaceGamePlayerController::EndPlay(EEndPlayReason::Type const reason) {
    lifecycle_ = Lifecycle::Ending;
    modal_ui_.detach_callbacks(*this);
    orchestrator_.Reset();
    detach_ship();
    set_control_context(levels::ControlContext::None);
    hud_.shutdown();
    hud_.shutdown_benchmark();
    modal_ui_.shutdown(*this);
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
    modal_ui_.on_ship_changed(true);

    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<UGameAudio>() : nullptr};
    if (IsValid(game)) {
        game->start_player_ship_ambience();
    } else {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::attach_ship: Game subsystem is unavailable; "
                    "player ship ambience will not play."));
    }
}
void ASpaceGamePlayerController::activate_ship_control() {
    if (lifecycle_ != Lifecycle::Playing || !can_activate_gameplay_control() ||
        !IsValid(Cast<Pawn>(GetPawn()))) {
        return;
    }

    find_orchestrator();
    modal_ui_.initialise_root(*this, ui_data);
    set_control_context(levels::ControlContext::Player);
    hud_.initialise(
        *this, orchestrator_.Get(), ui_data, levels::ControlContext::Player, viewer_speed_);
}
void ASpaceGamePlayerController::OnUnPossess() {
    detach_ship();
    SetActorTickEnabled(false);
    UE_LOG(LogSandbox, Display, TEXT("Unpossessed player ship"));
    Super::OnUnPossess();
}
void ASpaceGamePlayerController::detach_ship() {
    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<UGameAudio>() : nullptr};
    if (IsValid(game)) {
        game->stop_player_ship_ambience();
    }

    if (auto* const ship{Cast<Pawn>(GetPawn())}; IsValid(ship)) {
        ship->on_player_ship_died.Unbind();
    }

    if (control_context_ == levels::ControlContext::Player) {
        set_control_context(levels::ControlContext::None);
    }
    modal_ui_.on_ship_changed(false);
}
void ASpaceGamePlayerController::on_player_ship_died() {
    UnPossess();
}
void ASpaceGamePlayerController::find_orchestrator() {
    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::find_orchestrator: World is invalid."));
        return;
    }

    auto* const orchestrator{get_first_actor<ATestBatchOrchestrator>(*world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("ASpaceGamePlayerController::find_orchestrator: Orchestrator is invalid."));
        return;
    }

    orchestrator_ = orchestrator;
}

/* **************************************** */
// Input and presentation callbacks
/* **************************************** */
auto ASpaceGamePlayerController::activate_playerless_camera(ACameraActor& camera,
                                                            levels::ControlContext const context)
    -> bool {
    if (!can_activate_gameplay_control()) {
        return false;
    }

    if (context != levels::ControlContext::Observer &&
        context != levels::ControlContext::Benchmark) {
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

    find_orchestrator();
    viewer_camera_ = &camera;
    SetViewTarget(&camera);
    if (context == levels::ControlContext::Benchmark) {
        return activate_run_controls();
    }

    if (!set_control_context(context)) {
        return false;
    }

    hud_.shutdown_benchmark();
    if (!modal_ui_.initialise_root(*this, ui_data) ||
        !hud_.initialise(
            *this, orchestrator_.Get(), ui_data, levels::ControlContext::Observer, viewer_speed_)) {
        set_control_context(levels::ControlContext::None);
        return false;
    }

    SetActorTickEnabled(true);
    return true;
}
auto ASpaceGamePlayerController::activate_run_controls() -> bool {
    if (!can_activate_gameplay_control()) {
        return false;
    }

    find_orchestrator();
    if (!set_control_context(levels::ControlContext::Benchmark)) {
        return false;
    }

    hud_.shutdown();
    modal_ui_.shutdown(*this);
    if (!hud_.initialise_benchmark(*this, orchestrator_.Get(), ui_data)) {
        set_control_context(levels::ControlContext::None);
        return false;
    }

    SetActorTickEnabled(true);
    return true;
}
auto ASpaceGamePlayerController::set_control_context(levels::ControlContext context) -> bool {
    if (context == levels::ControlContext::Player && !IsValid(Cast<Pawn>(GetPawn()))) {
        return false;
    }
    if (context == levels::ControlContext::Observer && !viewer_camera_.IsValid()) {
        return false;
    }
    if (context != levels::ControlContext::Player) {
        present_steering({});
    }
    control_context_ = context;
    return true;
}
void ASpaceGamePlayerController::present_menu(host::MenuMessage const& message) {
    switch (unreal_endpoint().menu().screen) {
        case MenuScreen::Main:
            if (auto* main{modal_ui_.main_menu()}) {
                if (message.update) {
                    main->present(*message.update);
                }
            }
            break;
        case MenuScreen::Pause:
            if (auto* pause{modal_ui_.pause_menu()}) {
                pause->present(message);
            }
            break;
        case MenuScreen::Completion:
            break;
        case MenuScreen::Closed:
            break;
    }
}

void ASpaceGamePlayerController::present_steering(TOptional<FVector2D> position) {
    hud_.set_mouse_steering(position);
}
void ASpaceGamePlayerController::present_viewer_motion(host::ViewerMotion const& motion,
                                                       double dt) {
    if (control_context_ != levels::ControlContext::Observer) {
        return;
    }
    auto& camera{*viewer_camera_.Get()};
    if (viewer_speed_ != motion.speed) {
        viewer_speed_ = motion.speed;
        set_observer_movement_speed(viewer_speed_);
    }
    if (motion.yaw != 0.f || motion.pitch != 0.f) {
        auto rotation{camera.GetActorRotation()};
        rotation.Yaw += motion.yaw;
        rotation.Pitch = FMath::ClampAngle(rotation.Pitch + motion.pitch, -89., 89.);
        rotation.Roll = 0.;
        camera.SetActorRotation(rotation);
    }
    auto const planar{(camera.GetActorRightVector() * motion.right +
                       camera.GetActorForwardVector() * motion.forward)
                          .GetClampedToMaxSize(1.)};
    camera.AddActorWorldOffset((planar + FVector::UpVector * motion.up) * motion.speed * dt);
}
void ASpaceGamePlayerController::set_observer_movement_speed(float const speed) {
    hud_.set_observer_speed(speed);
}

/* **************************************** */
// Modal and simulation transitions
/* **************************************** */
auto ASpaceGamePlayerController::close_pause() -> bool {
    auto* widget{modal_ui_.pause_menu()};
    if (!IsValid(widget)) {
        UE_LOG(LogSandboxController, Error, TEXT("Cannot close a missing pause menu."));
        return false;
    }
    widget->close();
    return true;
}
auto ASpaceGamePlayerController::open_pause() -> bool {
    if (!is_gameplay_mode() || modal_ui_.has_modal()) {
        UE_LOG(
            LogSandboxController, Error, TEXT("Cannot present a pause menu in the current view."));
        return false;
    }

    auto const restore_context{get_active_control_context() == levels::ControlContext::None &&
                                       IsValid(Cast<Pawn>(GetPawn()))
                                   ? levels::ControlContext::Player
                                   : get_active_control_context()};
    modal_ui_.begin_suspend(restore_context);
    if (!set_control_context(levels::ControlContext::None)) {
        modal_ui_.clear_resume();
        return false;
    }

    hud_.hide_for_modal();
    if (!modal_ui_.show_pause(*this)) {
        restore_gameplay_presentation();
        return false;
    }
    return true;
}
void ASpaceGamePlayerController::restore_gameplay_presentation() {
    if (!is_gameplay_mode()) {
        return;
    }

    auto* const orchestrator{orchestrator_.Get()};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::restore_gameplay_presentation: Orchestrator is "
                    "invalid."));
        return;
    }

    if (orchestrator->get_state() == sim::LevelState::Stopped) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("ASpaceGamePlayerController::restore_gameplay_presentation: Orchestrator is "
                    "stopped."));
        return;
    }

    auto const target_context{modal_ui_.resume_context(IsValid(Cast<Pawn>(GetPawn()))
                                                           ? levels::ControlContext::Player
                                                           : levels::ControlContext::None)};
    if (!set_control_context(target_context)) {
        return;
    }

    hud_.restore_after_modal();
    modal_ui_.clear_resume();
    if (target_context == levels::ControlContext::Player) {
        hud_.initialise(*this, orchestrator, ui_data, target_context, viewer_speed_);
    }
}
void ASpaceGamePlayerController::on_pause_menu_deactivated() {
    modal_ui_.close_pause(*this);
    if (is_gameplay_mode()) {
        restore_gameplay_presentation();
    }
}
void ASpaceGamePlayerController::on_completion_menu_deactivated() {
    modal_ui_.close_completion(*this);
}
void ASpaceGamePlayerController::finish_level() {
    modal_ui_.clear_modals(*this);
    set_control_context(levels::ControlContext::None);
    hud_.shutdown();
    hud_.shutdown_benchmark();
}
void ASpaceGamePlayerController::show_level_completion(FLevelCompletion const& completion) {
    if (!is_gameplay_mode()) {
        return;
    }

    if (modal_ui_.initialise_root(*this, ui_data)) {
        modal_ui_.show_completion(*this, completion);
    }
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

} // namespace ioj
