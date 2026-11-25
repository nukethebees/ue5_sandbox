#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "GameHost.h"

#include "SpaceGame/simulation/TestBatchOrchestrator.h"
#include "SpaceGame/UnrealMessages.h"
#include <ioj/host/game_host_api.h>
#include <ioj/host/level_host.h>
#include <ioj/host/mission_startup.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/control_context.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/launch_options.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/screen_resolution.h>
#include <SpaceGame/settings/GameVideoSettings.h>
#include <SpaceGamePresentation/audio/GameAudio.h>

#include <SandboxCoreEngine/strings.h>

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/Platform.h"
#include "HAL/PlatformApplicationMisc.h" // IWYU pragma: keep
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Slate/SceneViewport.h"
#include "UnrealEngine.h"
#include "Widgets/SWindow.h"
#include <Engine/GameInstance.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace ioj {
namespace {
TWeakObjectPtr<ATestBatchOrchestrator> level_world;
MissionStartup mission_status{MissionStartup::Pending};
}
void attach_level_world(ATestBatchOrchestrator& actor) {
    check(!level_world.IsValid());
    level_world = &actor;
}
void detach_level_world(ATestBatchOrchestrator& actor) {
    if (level_world.Get(true) == &actor) {
        level_world.Reset();
    }
}
namespace host {
auto level_world_ready() -> bool {
    return level_world.IsValid();
}
void open_mission() {
    level_world.Reset();
    mission_status = MissionStartup::Pending;
    UGameplayStatics::OpenLevel(GEngine->GameViewport->GetWorld(),
                                TEXT("/SpaceGame/Levels/GameRuntime"));
}
void open_menu(std::string_view preferred_level_id, std::string_view error) {
    menu_selection().emplace(levels::LevelId{std::string{preferred_level_id}}, std::string{error});
    level_world.Reset();
    UGameplayStatics::OpenLevel(GEngine->GameViewport->GetWorld(),
                                TEXT("/SpaceGame/Levels/MainMenu"));
}
auto prepare_level(session::PendingLevel const& pending,
                   std::optional<sim::player::PlayerSpawnData>& player) -> bool {
    auto result{level_world->prepare_level(pending, player)};
    if (!result) {
        level_world->handle_level_start_failure(
            result.error().format(to_fstring(pending.source_path)));
    }

    return result.has_value();
}
auto static_geometry(sim::collision::CollisionGridSpec grid)
    -> std::span<sim::collision::WorldAABB const> {
    return level_world->static_geometry(grid);
}
auto bind_level(session::PendingLevel const& pending, sim::LevelSim const& level) -> bool {
    return level_world->bind_level(pending, level);
}
void level_start_failed(std::string_view error) {
    level_world->handle_level_start_failure(to_fstring(error));
}
auto presents_level(sim::LevelSim const& level) -> bool {
    return level_world.IsValid() && level_world->get_level_simulation() == &level;
}
auto take_frame_delta_seconds() -> std::optional<double> {
    return level_world->take_frame_delta_seconds();
}

void present_level(double delta_seconds) {
    level_world->present_level(delta_seconds);
}
void simulation_paused() {
    level_world->on_simulation_paused.Broadcast();
}
void finish_level(profiles::CompletedLevel const& completed,
                  bool scored,
                  bool new_best,
                  bool show_results) {
    if (level_world.IsValid() && level_world->get_level_simulation()) {
        level_world->finish_level(completed, scored, new_best, show_results);
    }
}
}
auto IojPlayer_MissionStartup() -> MissionStartup {
    return mission_status;
}
void report_mission_startup(MissionStartup status) {
    mission_status = status;
}
void IojPlayer_Detach() {
    detach_player();
    mission_status = MissionStartup::Pending;
}

auto IojPlayer_ApplyPreferences() -> bool {
    auto const& values{player_settings().applied()};
    if (auto result{FGameVideoSettings{}.apply_quality(video_settings(values))}; !result) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Applying video preferences failed: %s"),
               *to_fstring(result.error()));
        return false;
    }

    auto* audio{GEngine->GameViewport->GetWorld()->GetGameInstance()->GetSubsystem<UGameAudio>()};
    check(audio);
    audio->set_master_volume(values.master_volume);
    audio->set_music_volume(values.music_volume);
    audio->set_sfx_volume(values.sfx_volume);
    audio->set_ui_volume(values.ui_volume);
    return true;
}

auto IojPlayer_GameFailed() -> bool {
    return GIsCriticalError;
}

auto IojPlayer_GameReady() -> bool {
    auto const* viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto const* world{viewport ? viewport->GetWorld() : nullptr};
    return world && world->HasBegunPlay();
}

auto IojPlayer_ExitRequested() -> bool {
    return IsEngineExitRequested();
}

void IojPlayer_BeginFrame() {
    clear_consumed_messages();
    FPlatformApplicationMisc::PumpMessages(true);
}

auto IojPlayer_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool {
    auto const window{GEngine->GameViewport->GetWindow().ToSharedRef()};
    auto const native_window{window->GetNativeWindow().ToSharedRef()};
    auto* viewport{GEngine->GameViewport->GetGameViewport()};
    auto& slate{FSlateApplication::Get()};

    GUELibraryOverrideSettings.WindowWidth = width;
    GUELibraryOverrideSettings.WindowHeight = height;
    GSystemResolution.ResX = width;
    GSystemResolution.ResY = height;

    slate.OnMovedWindow(native_window, x, y);
    slate.OnSizeChanged(native_window, width, height);
    if (viewport && viewport->GetSizeXY() != FIntPoint{width, height}) {
        viewport->SetViewportSize(width, height);
        window->SetViewportSizeDrivenByWindow(true);
    }

    if (activate) {
        slate.ProcessApplicationActivationEvent(native_window->IsForegroundWindow());
    }

    auto const measured{window->GetSizeInScreen()};
    return measured.X == width && measured.Y == height && viewport &&
           viewport->GetSizeXY() == FIntPoint{width, height};
}

}
#endif
