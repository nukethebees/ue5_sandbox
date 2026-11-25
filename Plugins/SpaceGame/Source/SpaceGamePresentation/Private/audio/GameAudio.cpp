#include <SpaceGamePresentation/audio/GameAudio.h>

#include <ioj/files.h>
#include <SpaceGamePresentation/audio/AudioSourceLibrary.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <Components/AudioComponent.h>
#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>
#include <Sound/SoundWave.h>
#include <UObject/UObjectGlobals.h>

namespace ioj::game_audio {
inline constexpr TCHAR menu_ambience_object_path[]{
    TEXT("/SpaceGame/Audio/Generated/A_MenuAmbience_ComputerRoomLow02."
         "A_MenuAmbience_ComputerRoomLow02")};
inline constexpr TCHAR menu_button_pressed_object_path[]{
    TEXT("/SpaceGame/Audio/Generated/A_MenuButtonPressed.A_MenuButtonPressed")};
inline constexpr TCHAR player_ship_ambience_object_path[]{
    TEXT("/SpaceGame/Audio/Generated/A_PlayerShipAmbience_SpaceshipHigh02."
         "A_PlayerShipAmbience_SpaceshipHigh02")};
inline constexpr float menu_button_playback_seconds{0.5f};
}

namespace ioj {
FGameAudioFacade::FGameAudioFacade(UGameAudio& audio)
    : audio_{&audio} {}

void FGameAudioFacade::play_button_pressed() const {
    if (audio_ == nullptr) {
        return;
    }

    audio_->play_button_pressed();
}

void FGameAudioFacade::play_player_laser() const {
    if (audio_) {
        audio_->play_player_laser();
    }
}

void UGameAudio::play_player_laser() {
    if (IsValid(GetGameInstance())) {
        UGameplayStatics::PlaySound2D(
            GetGameInstance(), player_laser_sound_.Get(), master_volume_ * sfx_volume_);
    }
}

void FGameAudioFacade::play_entity_death(FVector const location) const {
    if (audio_) {
        audio_->play_entity_death(location);
    }
}

void UGameAudio::play_entity_death(FVector const location) {
    if (death_impact_sounds_.IsEmpty() || !IsValid(GetGameInstance())) {
        return;
    }

    auto* const sound{death_impact_sounds_[death_impact_indices_[next_impact_]].Get()};
    next_impact_ = (next_impact_ + 1) % death_impact_indices_.size();
    UGameplayStatics::PlaySoundAtLocation(GetGameInstance(),
                                          sound,
                                          location,
                                          master_volume_ * sfx_volume_,
                                          1.f,
                                          0.f,
                                          death_attenuation_.Get());
}

void UGameAudio::Deinitialize() {
    stop_menu_ambience();
    stop_player_ship_ambience();
    stop_button_audio();
    Super::Deinitialize();
}
void UGameAudio::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    external_audio_ = resolve_audio_source_library(get_configured_audio_root());
    if (external_audio_.root.path.empty()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("BEE_AUDIO_ROOT is unset. External audio sources are unavailable."));
    } else if (!external_audio_.root.valid) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("BEE_AUDIO_ROOT is not an existing directory: \"%s\". External audio "
                    "sources are unavailable."),
               *ioj::to_fstring(ioj::path_to_utf8(external_audio_.root.path)));
    } else {
        auto all_directories_valid{true};
        for (auto const& directory : external_audio_.directories) {
            if (directory.valid) {
                continue;
            }

            all_directories_valid = false;
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Expected external audio source directory is missing or is not a "
                        "directory: \"%s\" (relative directory \"%s\")."),
                   *ioj::to_fstring(ioj::path_to_utf8(directory.path)),
                   *ioj::to_fstring(ioj::path_to_utf8(directory.relative_path)));
        }

        if (all_directories_valid && !external_audio_.menu_ambience_source.valid) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Expected menu ambience source file is missing: \"%s\" (relative path "
                        "\"%s\")."),
                   *ioj::to_fstring(ioj::path_to_utf8(external_audio_.menu_ambience_source.path)),
                   *ioj::to_fstring(
                       ioj::path_to_utf8(external_audio_.menu_ambience_source.relative_path)));
        }

        if (all_directories_valid && !external_audio_.menu_button_pressed_source.valid) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Expected menu button source file is missing: \"%s\" (relative path "
                        "\"%s\")."),
                   *ioj::to_fstring(
                       ioj::path_to_utf8(external_audio_.menu_button_pressed_source.path)),
                   *ioj::to_fstring(ioj::path_to_utf8(
                       external_audio_.menu_button_pressed_source.relative_path)));
        }

        if (all_directories_valid && !external_audio_.player_ship_ambience_source.valid) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Expected player ship ambience source file is missing: \"%s\" (relative "
                        "path \"%s\")."),
                   *ioj::to_fstring(
                       ioj::path_to_utf8(external_audio_.player_ship_ambience_source.path)),
                   *ioj::to_fstring(ioj::path_to_utf8(
                       external_audio_.player_ship_ambience_source.relative_path)));
        }
    }

    death_impact_sounds_.Reset();
    auto const impact_count{static_cast<int32>(external_audio_.death_impact_sources.size())};
    for (int32 index{}; index < impact_count; ++index) {
        auto const path{FString::Printf(
            TEXT("/SpaceGame/Audio/Generated/A_EntityDeathImpact_%d.A_EntityDeathImpact_%d"),
            index,
            index)};
        if (auto* const sound{LoadObject<USoundWave>(nullptr, *path, {}, LOAD_NoWarn)}) {
            death_impact_sounds_.Emplace(sound);
        } else {
            UE_LOG(LogSandboxAudio, Warning, TEXT("Optional impact sound is missing: %s"), *path);
        }
    }

    next_impact_ = 0;
    if (!death_impact_sounds_.IsEmpty()) {
        FRandomStream random;
        random.GenerateNewSeed();
        for (auto& index : death_impact_indices_) {
            index = static_cast<std::uint8_t>(random.RandHelper(death_impact_sounds_.Num()));
        }

        death_attenuation_.Reset(NewObject<USoundAttenuation>());
        auto& settings{death_attenuation_->Attenuation};
        settings.bAttenuate = true;
        settings.bSpatialize = true;
        settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
        settings.AttenuationShape = EAttenuationShape::Sphere;
        settings.AttenuationShapeExtents = FVector{20000.f};
        settings.FalloffDistance = 480000.f;
        settings.StereoSpread = 0.f;
    }

    player_laser_sound_.Reset(LoadObject<USoundWave>(
        nullptr, TEXT("/SpaceGame/Audio/Generated/A_PlayerLaser.A_PlayerLaser"), {}, LOAD_NoWarn));
    if (!player_laser_sound_.IsValid()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Optional player laser sound is missing. Run the import-game-audio workflow."));
    }

    auto* const menu_ambience{
        LoadObject<USoundWave>(nullptr, game_audio::menu_ambience_object_path, {}, LOAD_NoWarn)};
    menu_ambience_sound_.Reset(menu_ambience);
    if (!menu_ambience_sound_.IsValid()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Optional menu ambience asset is unavailable: \"%s\". Run `cmake "
                    "--workflow --preset import-game-audio` to import it locally."),
               game_audio::menu_ambience_object_path);
    }

    auto* const menu_button_pressed{LoadObject<USoundWave>(
        nullptr, game_audio::menu_button_pressed_object_path, {}, LOAD_NoWarn)};
    menu_button_pressed_sound_.Reset(menu_button_pressed);
    if (!menu_button_pressed_sound_.IsValid()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Optional menu button asset is unavailable: \"%s\". Run `cmake --workflow "
                    "--preset import-game-audio` to import it locally."),
               game_audio::menu_button_pressed_object_path);
    }

    auto* const player_ship_ambience{LoadObject<USoundWave>(
        nullptr, game_audio::player_ship_ambience_object_path, {}, LOAD_NoWarn)};
    player_ship_ambience_sound_.Reset(player_ship_ambience);
    if (!player_ship_ambience_sound_.IsValid()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Optional player ship ambience asset is unavailable: \"%s\". Run `cmake "
                    "--workflow --preset import-game-audio` to import it locally."),
               game_audio::player_ship_ambience_object_path);
    }
}

void UGameAudio::start_menu_ambience() {
    auto* const active_component{menu_ambience_component_.Get()};
    if (IsValid(active_component) && active_component->IsPlaying()) {
        return;
    }

    stop_menu_ambience();
    auto* const menu_ambience{menu_ambience_sound_.Get()};
    if (!IsValid(menu_ambience)) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    if (!IsValid(game_instance)) {
        return;
    }

    constexpr bool persist_across_level_transition{false};
    constexpr bool auto_destroy{false};
    auto* const component{UGameplayStatics::SpawnSound2D(game_instance,
                                                         menu_ambience,
                                                         master_volume_ * music_volume_,
                                                         1.0f,
                                                         0.0f,
                                                         nullptr,
                                                         persist_across_level_transition,
                                                         auto_destroy)};
    if (!IsValid(component)) {
        if (!ambience_playback_warning_logged_) {
            UE_LOG(LogSandboxAudio, Warning, TEXT("Could not start optional menu ambience audio."));
            ambience_playback_warning_logged_ = true;
        }

        return;
    }

    menu_ambience_component_ = component;
}

void UGameAudio::play_button_pressed() {
    stop_button_audio();

    auto* const sound{menu_button_pressed_sound_.Get()};
    if (!IsValid(sound)) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    if (!IsValid(game_instance)) {
        return;
    }

    constexpr bool persist_across_level_transition{false};
    constexpr bool auto_destroy{false};
    auto* const component{UGameplayStatics::CreateSound2D(game_instance,
                                                          sound,
                                                          master_volume_ * ui_volume_,
                                                          1.0f,
                                                          0.0f,
                                                          nullptr,
                                                          persist_across_level_transition,
                                                          auto_destroy)};
    if (!IsValid(component)) {
        if (!button_playback_warning_logged_) {
            UE_LOG(LogSandboxAudio, Warning, TEXT("Could not start optional menu button audio."));
            button_playback_warning_logged_ = true;
        }

        return;
    }

    menu_button_pressed_component_ = component;
    component->Play(0.0f);
    component->StopDelayed(game_audio::menu_button_playback_seconds);
}

void UGameAudio::start_player_ship_ambience() {
    auto* const active_component{player_ship_ambience_component_.Get()};
    if (IsValid(active_component) && active_component->IsPlaying()) {
        return;
    }

    stop_player_ship_ambience();
    auto* const ambience{player_ship_ambience_sound_.Get()};
    if (!IsValid(ambience)) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    if (!IsValid(game_instance)) {
        return;
    }

    constexpr bool persist_across_level_transition{false};
    constexpr bool auto_destroy{false};
    auto* const component{UGameplayStatics::SpawnSound2D(game_instance,
                                                         ambience,
                                                         master_volume_ * sfx_volume_,
                                                         1.0f,
                                                         0.0f,
                                                         nullptr,
                                                         persist_across_level_transition,
                                                         auto_destroy)};
    if (!IsValid(component)) {
        if (!player_ship_ambience_playback_warning_logged_) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Could not start optional player ship ambience audio."));
            player_ship_ambience_playback_warning_logged_ = true;
        }

        return;
    }

    player_ship_ambience_component_ = component;
}

void UGameAudio::stop_menu_ambience() {
    auto* const component{menu_ambience_component_.Get()};
    if (IsValid(component)) {
        component->Stop();
        component->DestroyComponent();
    }

    menu_ambience_component_.Reset();
}

void UGameAudio::stop_player_ship_ambience() {
    auto* const component{player_ship_ambience_component_.Get()};
    if (IsValid(component)) {
        component->Stop();
        component->DestroyComponent();
    }

    player_ship_ambience_component_.Reset();
}

void UGameAudio::stop_button_audio() {
    auto* const component{menu_button_pressed_component_.Get()};
    if (IsValid(component)) {
        component->Stop();
        component->DestroyComponent();
    }

    menu_button_pressed_component_.Reset();
}

void UGameAudio::set_master_volume(float const volume) {
    master_volume_ = FMath::Clamp(volume, 0.0f, 1.0f);

    auto* const menu_ambience_component{menu_ambience_component_.Get()};
    if (IsValid(menu_ambience_component)) {
        menu_ambience_component->SetVolumeMultiplier(master_volume_ * music_volume_);
    }

    auto* const menu_button_pressed_component{menu_button_pressed_component_.Get()};
    if (IsValid(menu_button_pressed_component)) {
        menu_button_pressed_component->SetVolumeMultiplier(master_volume_ * ui_volume_);
    }

    auto* const player_ship_ambience_component{player_ship_ambience_component_.Get()};
    if (IsValid(player_ship_ambience_component)) {
        player_ship_ambience_component->SetVolumeMultiplier(master_volume_ * sfx_volume_);
    }
}

void UGameAudio::set_music_volume(float const volume) {
    music_volume_ = FMath::Clamp(volume, 0.0f, 1.0f);
    auto* const component{menu_ambience_component_.Get()};
    if (IsValid(component)) {
        component->SetVolumeMultiplier(master_volume_ * music_volume_);
    }
}

void UGameAudio::set_ui_volume(float const volume) {
    ui_volume_ = FMath::Clamp(volume, 0.0f, 1.0f);
    auto* const component{menu_button_pressed_component_.Get()};
    if (IsValid(component)) {
        component->SetVolumeMultiplier(master_volume_ * ui_volume_);
    }
}

void UGameAudio::set_sfx_volume(float const volume) {
    sfx_volume_ = FMath::Clamp(volume, 0.0f, 1.0f);
    auto* const player_ship_ambience_component{player_ship_ambience_component_.Get()};
    if (IsValid(player_ship_ambience_component)) {
        player_ship_ambience_component->SetVolumeMultiplier(master_volume_ * sfx_volume_);
    }
}

auto UGameAudio::facade() -> FGameAudioFacade {
    return FGameAudioFacade{*this};
}

auto UGameAudio::external_audio_available() const noexcept -> bool {
    if (!external_audio_.root.valid || !external_audio_.menu_ambience_source.valid ||
        !external_audio_.menu_button_pressed_source.valid ||
        !external_audio_.player_ship_ambience_source.valid ||
        !external_audio_.player_laser_source.valid) {
        return false;
    }

    for (auto const& source : external_audio_.death_impact_sources) {
        if (!source.valid) {
            return false;
        }
    }

    for (auto const& directory : external_audio_.directories) {
        if (!directory.valid) {
            return false;
        }
    }

    return true;
}
}
