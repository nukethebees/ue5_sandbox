#include "SpaceGame/UnrealEndpoint.h"

#include "input/HostInput.h"
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
auto unreal_endpoint() -> host::UnrealEndpoint& {
    static host::UnrealEndpoint endpoint;
    return endpoint;
}
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
namespace {
#if !UE_BUILD_SHIPPING
FAutoConsoleCommand reset_profile_command{
    TEXT("spacegame_reset_test_profile"),
    TEXT("Replaces the dedicated Test Profile and activates it."),
    FConsoleCommandDelegate::CreateLambda(
        [] { unreal_endpoint().send(host::UnrealMessageKind::ResetTestProfile); })};
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
void host::UnrealEndpoint::level_world_ready(ATestBatchOrchestrator& actor) {
    check(!level_world_);
    level_world_ = &actor;
    send(UnrealMessageKind::LevelWorldReady);
}
void host::UnrealEndpoint::release_world(ATestBatchOrchestrator& actor) {
    if (level_world_ == &actor) {
        level_world_ = nullptr;
        send(UnrealMessageKind::LevelWorldReleased);
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
                menu_ = message.state;
                if (player) {
                    player->present_menu(message);
                }
                break;
            }
            case OpenPause:
                if (!player || !player->open_pause()) {
                    send(UnrealMessageKind::PausePresentationFailed);
                }
                break;
            case ClosePause:
                if (!player || !player->close_pause()) {
                    send(UnrealMessageKind::PausePresentationFailed);
                }
                break;
            case Benchmark:
                if (auto* hud{player->get_benchmark_hud()}) {
                    hud->select_button(messages.benchmark_selections[index]);
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
                keyboard_ = messages.keyboards[index].get();
                break;
            case Capabilities:
                capabilities_ = messages.capabilities[index].get();
                break;
            case Settings: {
                auto const& update{messages.settings[index]};
                settings_ = update.values;
                resolutions_.assign(update.resolutions.begin(), update.resolutions.end());
                break;
            }
            case Catalog:
                if (auto* main{player ? player->main_menu() : nullptr}) {
                    if (auto* widget{main->level_select()}) {
                        widget->present_catalog(messages.catalogs[index]);
                        menu_error_.clear();
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
                menu_error_ = messages.errors[index];
                UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/MainMenu"));
                break;
            case OpenLevel:
                UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/GameRuntime"));
                break;
            case InitializeLevel: {
                auto const& init{messages.initializations[index]};
                if (!level_world_) {
                    level_initialized("Level world was released before initialization");
                    break;
                }
                auto result{level_world_->initialize_level(init.level, init.frame)};
                std::string error;
                if (!result) {
                    auto const message{
                        result.error().format(to_fstring(init.level.setup.source_path))};
                    UE_LOG(LogTemp, Error, TEXT("%s"), *message);
                    level_world_->release_level();
                    error = to_string(message);
                } else if (auto* hud{player->get_benchmark_hud()}) {
                    hud->present_status(init.frame.status);
                }
                level_initialized(std::move(error));
                break;
            }
            case Frame: {
                auto const& frame{messages.frames[index]};
                auto const& previous{level_world_->status()};
                auto const& status{frame.frame.status};
                if (previous.state != status.state ||
                    previous.ticks_remaining != status.ticks_remaining) {
                    if (auto* hud{player->get_benchmark_hud()}) {
                        hud->present_status(status);
                    }
                }
                level_world_->present_level(frame.seconds, frame.frame);
                break;
            }
            case Finish: {
                auto const& result{messages.completions[index]};
                if (level_world_ && level_world_->has_level()) {
                    level_world_->finish_level(
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
}
void IojPlayer_ReportEngineResult() {
    if (GIsCriticalError) {
        unreal_endpoint().send(host::UnrealMessageKind::CriticalError);
    } else if (IsEngineExitRequested()) {
        unreal_endpoint().send(host::UnrealMessageKind::ExitRequested);
    }
}
void IojPlayer_Exchange(host::NativeMessages const& outgoing, host::UnrealMessages& incoming) {
    auto& endpoint{unreal_endpoint()};
    endpoint.receive(outgoing);
    incoming = endpoint.messages();
}
void IojPlayer_ReleaseMessages() {
    unreal_endpoint().clear();
}
void IojPlayer_Detach() {
    unreal_endpoint() = {};
}
#endif
}
