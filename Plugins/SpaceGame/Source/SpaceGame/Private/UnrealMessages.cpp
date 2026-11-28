#include "SpaceGame/UnrealMessages.h"

#include "settings/GameHost.h"
#include "UnrealEndpoint.h"
#include <ioj/host/game_host_api.h>

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include <SpaceGame/settings/GameVideoSettings.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <Engine/GameViewportClient.h>
#include <Engine/World.h>
#include <HAL/IConsoleManager.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/App.h>
#include <Misc/MessageDialog.h>
#include <UnrealEngine.h>
#endif

namespace ioj {
namespace {
host::UnrealEndpoint endpoint;
MenuPresentation menu;
PlayerSettingsView settings_view;
KeyboardLayout keyboard;
SystemCapabilities capabilities;
std::vector<ScreenResolution> resolutions;
std::string menu_error;
}

auto menu_presentation() -> MenuPresentation const& {
    return menu;
}
auto player_settings() -> PlayerSettingsView const& {
    return settings_view;
}
auto system_capabilities() -> SystemCapabilities const& {
    return capabilities;
}
auto keyboard_layout() -> KeyboardLayout const& {
    return keyboard;
}
auto supported_resolutions() -> std::span<ScreenResolution const> {
    return resolutions;
}
auto launch_error() -> std::string_view {
    return menu_error;
}
void hit_menu_control(MenuTarget target) {
    endpoint.hit(std::move(target));
}
void edit_menu_text(MenuControl control, std::string value) {
    endpoint.edit_text({control, std::move(value)});
}
void edit_setting(GameSetting setting, GameSettingValue value) {
    endpoint.edit_setting({setting, std::move(value)});
}
void request_level_catalog() {
    endpoint.send(host::UnrealMessageKind::RefreshCatalog);
}
void request_profile_refresh() {
    endpoint.send(host::UnrealMessageKind::RefreshProfiles);
}
void report_pause_interaction(host::PauseWidgetEvent event) {
    endpoint.pause_interaction(event);
}
void request_player_action(host::PlayerAction action) {
    endpoint.player_action(action);
}
void request_simulation_mode(levels::LaunchMode mode) {
    endpoint.request_simulation_mode(mode);
}

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
namespace {
TWeakObjectPtr<ATestBatchOrchestrator> level_world;
std::optional<host::EngineStatus> reported_status;
#if !UE_BUILD_SHIPPING
FAutoConsoleCommand reset_profile_command{
    TEXT("spacegame_reset_test_profile"),
    TEXT("Replaces the dedicated Test Profile and activates it."),
    FConsoleCommandDelegate::CreateLambda(
        [] { endpoint.send(host::UnrealMessageKind::ResetTestProfile); })};
#endif

auto apply_preferences(host::ApplySettings const& edit) -> std::string {
    if (edit.category == GameSettingCategory::Video) {
        if (auto result{FGameVideoSettings{}.apply_quality(edit.values.video)}; !result) {
            return result.error();
        }
    } else if (edit.category == GameSettingCategory::Audio) {
        auto* audio{
            GEngine->GameViewport->GetWorld()->GetGameInstance()->GetSubsystem<UGameAudio>()};
        check(audio);
        audio->set_master_volume(edit.values.audio.master_volume);
        audio->set_music_volume(edit.values.audio.music_volume);
        audio->set_sfx_volume(edit.values.audio.sfx_volume);
        audio->set_ui_volume(edit.values.audio.ui_volume);
    }
    return {};
}
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

void host::UnrealEndpoint::receive(host::NativeMessages const& messages) {
    auto* const viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto* const world{viewport ? viewport->GetWorld() : nullptr};
    auto* const player{world ? Cast<ASpaceGamePlayerController>(world->GetFirstPlayerController())
                             : nullptr};
    for (auto const [kind, index] : messages.order) {
        using enum host::NativeMessageKind;
        switch (kind) {
            case Key:
                host::present_input(messages.keys[index]);
                break;
            case Text:
                host::present_input(messages.texts[index]);
                break;
            case Pointer:
                host::present_input(messages.pointers[index]);
                break;
            case Menu: {
                auto const& message{messages.menus[index]};
                menu = message.state;
                if (player && message.update) {
                    player->present_menu(*message.update);
                }
                break;
            }
            case Pause:
                pause_presented(player && player->present_pause(messages.pause_commands[index]));
                break;
            case Benchmark:
                if (auto* hud{player->get_benchmark_hud()}) {
                    hud->select_button(messages.benchmark_selections[index] != 0);
                }
                break;
            case Viewer: {
                auto const& step{messages.viewer_steps[index]};
                player->present_viewer_motion(step.motion, step.seconds);
                break;
            }
            case Steering: {
                TOptional<FVector2D> position;
                if (auto direction{messages.steering[index]}) {
                    position.Emplace(direction->x(), direction->y());
                }
                player->present_steering(position);
                break;
            }
            case Keyboard:
                keyboard = messages.keyboards[index].get();
                break;
            case Capabilities:
                capabilities = messages.capabilities[index].get();
                break;
            case Settings: {
                auto const& update{messages.settings[index]};
                settings_view = update.values;
                resolutions.assign(update.resolutions.begin(), update.resolutions.end());
                break;
            }
            case Catalog:
                if (auto* main{player ? player->main_menu() : nullptr}) {
                    if (auto* widget{main->level_select()}) {
                        widget->present_catalog(messages.catalogs[index]);
                        menu_error.clear();
                    }
                }
                break;
            case Profiles:
                if (auto* main{player ? player->main_menu() : nullptr}) {
                    if (auto* widget{main->save_game_viewer()}) {
                        widget->present_profiles(messages.profiles[index]);
                    }
                }
                break;
            case ProfileResult:
                if (auto* main{player ? player->main_menu() : nullptr}) {
                    if (auto* widget{main->save_game_viewer()}) {
                        widget->show_create_result(messages.profile_results[index]);
                    }
                }
                break;
            case OpenMenu:
                menu_error = messages.errors[index];
                level_world.Reset();
                UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/MainMenu"));
                break;
            case OpenLevel:
                level_world.Reset();
                UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/GameRuntime"));
                break;
            case InitializeLevel: {
                auto const& init{messages.initializations[index]};
                auto result{level_world->initialize_level(init.level, init.frame)};
                std::string error;
                if (!result) {
                    auto const message{
                        result.error().format(to_fstring(init.level.setup.source_path))};
                    UE_LOG(LogTemp, Error, TEXT("%s"), *message);
                    level_world->release_level();
                    error = to_string(message);
                }
                level_initialized(std::move(error));
                break;
            }
            case Frame: {
                auto const& frame{messages.frames[index]};
                level_world->present_level(frame.seconds, frame.frame);
                break;
            }
            case SimulationPaused:
                level_world->on_simulation_paused.Broadcast();
                break;
            case Finish: {
                auto const& result{messages.completions[index]};
                if (level_world.IsValid() && level_world->has_level()) {
                    level_world->finish_level(
                        result.level, result.scored, result.new_best, result.show_results);
                }
                break;
            }
            case ApplySettings: {
                auto const& edit{messages.preferences[index]};
                settings_applied({edit.operation, edit.category, apply_preferences(edit)});
                break;
            }
            case Error: {
                auto const message{FText::FromString(to_fstring(messages.errors[index]))};
                UE_LOG(LogTemp, Error, TEXT("%s"), *message.ToString());
                if (!FApp::IsUnattended() && !GIsAutomationTesting) {
                    FMessageDialog::Open(EAppMsgType::Ok, message);
                }
                break;
            }
        }
    }
    host::EngineStatus const current_status{world && world->HasBegunPlay(),
                                            level_world.IsValid(),
                                            level_world.IsValid() && level_world->has_level(),
                                            IsEngineExitRequested(),
                                            !!GIsCriticalError};
    if (current_status != reported_status) {
        reported_status = current_status;
        engine_status(current_status);
    }
}
void IojPlayer_Exchange(host::NativeMessages const& outgoing, host::UnrealMessages& incoming) {
    endpoint.receive(outgoing);
    incoming = endpoint.messages();
}
void IojPlayer_ReleaseMessages() {
    endpoint.clear();
}
void IojPlayer_Detach() {
    endpoint = {};
    reported_status.reset();
    level_world.Reset();
    menu = {};
    settings_view = {};
    resolutions.clear();
    menu_error.clear();
}
#endif
}
